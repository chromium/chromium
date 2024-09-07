// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions/permissions_updater.h"

#include <set>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/permissions/permissions_helpers.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/permissions.h"
#include "chrome/common/webui_url_constants.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/network_permissions_updater.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/common/cors_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/mojom/permission_set.mojom.h"
#include "extensions/common/mojom/renderer.mojom.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern_set.h"

using content::RenderProcessHost;
using extensions::permissions_api_helpers::PackPermissionSet;

namespace extensions {

namespace permissions = api::permissions;

namespace {

// A helper class to watch profile lifetime.
class PermissionsUpdaterShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  PermissionsUpdaterShutdownNotifierFactory(
      const PermissionsUpdaterShutdownNotifierFactory&) = delete;
  PermissionsUpdaterShutdownNotifierFactory& operator=(
      const PermissionsUpdaterShutdownNotifierFactory&) = delete;

  static PermissionsUpdaterShutdownNotifierFactory* GetInstance() {
    static base::NoDestructor<PermissionsUpdaterShutdownNotifierFactory>
        factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<PermissionsUpdaterShutdownNotifierFactory>;

  PermissionsUpdaterShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "PermissionsUpdaterShutdownFactory") {
    DependsOn(EventRouterFactory::GetInstance());
    DependsOn(ExtensionSystemFactory::GetInstance());
  }
  ~PermissionsUpdaterShutdownNotifierFactory() override {}
};

// Returns an URLPatternSet containing the sites that the user has indicated
// extensions are always allowed to run on.
URLPatternSet GetUserPermittedPatternSet(
    content::BrowserContext& browser_context) {
  PermissionsManager* permissions_manager =
      PermissionsManager::Get(&browser_context);
  URLPatternSet user_permitted_sites;
  for (const url::Origin& origin :
       permissions_manager->GetUserPermissionsSettings().permitted_sites) {
    user_permitted_sites.AddOrigin(Extension::kValidHostPermissionSchemes,
                                   origin);
  }

  return user_permitted_sites;
}

}  // namespace

// A helper class to asynchronously dispatch the event to notify policy host
// restrictions or permissions once they have been updated. This will fire the
// event if and only if the BrowserContext is still valid.
// This class manages its own lifetime and deletes itself when either the
// permissions updated event is fired, or the BrowserContext is shut down
// (whichever happens first).
// TODO(devlin): After having extracted much of this into
// NetworkPermissionsUpdater, this class is a glorified watcher for the
// profile lifetime (since it depends on things like EventRouter). This might
// be able to be replaced with a simple check if the profile is still valid in
// a free function.
class PermissionsUpdater::NetworkPermissionsUpdateHelper {
 public:
  NetworkPermissionsUpdateHelper(const NetworkPermissionsUpdateHelper&) =
      delete;
  NetworkPermissionsUpdateHelper& operator=(
      const NetworkPermissionsUpdateHelper&) = delete;

  static void UpdatePermissions(content::BrowserContext* browser_context,
                                EventType event_type,
                                scoped_refptr<const Extension> extension,
                                const PermissionSet& changed,
                                base::OnceClosure completion_callback);

  static void UpdateDefaultPolicyHostRestrictions(
      content::BrowserContext* browser_context,
      const URLPatternSet& default_runtime_blocked_hosts,
      const URLPatternSet& default_runtime_allowed_hosts);

 private:
  // This class manages its own lifetime.
  NetworkPermissionsUpdateHelper(content::BrowserContext* browser_context,
                                 base::OnceClosure dispatch_event);
  ~NetworkPermissionsUpdateHelper();

  void OnShutdown();
  void OnOriginAccessUpdated();

  base::OnceClosure dispatch_event_;
  base::CallbackListSubscription shutdown_subscription_;
  base::WeakPtrFactory<NetworkPermissionsUpdateHelper> weak_factory_{this};
};

// static
void PermissionsUpdater::NetworkPermissionsUpdateHelper::UpdatePermissions(
    content::BrowserContext* browser_context,
    EventType event_type,
    scoped_refptr<const Extension> extension,
    const PermissionSet& changed,
    base::OnceClosure completion_callback) {
  // If there is no difference in allowlist/blocklist for the extension, we can
  // synchronously finish it without updating the CORS access list.
  // We do not apply this optimization for POLICY event_type, since callers do
  // not pass effective |changed| argument.
  if (event_type != POLICY && changed.effective_hosts().is_empty()) {
    PermissionsUpdater::NotifyPermissionsUpdated(
        browser_context, event_type, std::move(extension), changed.Clone(),
        std::move(completion_callback));
    return;
  }

  NetworkPermissionsUpdateHelper* helper = new NetworkPermissionsUpdateHelper(
      browser_context,
      base::BindOnce(&PermissionsUpdater::NotifyPermissionsUpdated,
                     browser_context, event_type, extension, changed.Clone(),
                     std::move(completion_callback)));

  // After an asynchronous call below, the helper will call
  // NotifyPermissionsUpdated if the profile is still valid.
  NetworkPermissionsUpdater::UpdateExtension(
      *browser_context, *extension,
      NetworkPermissionsUpdater::ContextSet::kAllRelatedContexts,
      base::BindOnce(&NetworkPermissionsUpdateHelper::OnOriginAccessUpdated,
                     helper->weak_factory_.GetWeakPtr()));
}

// static
void PermissionsUpdater::NetworkPermissionsUpdateHelper::
    UpdateDefaultPolicyHostRestrictions(
        content::BrowserContext* browser_context,
        const URLPatternSet& default_runtime_blocked_hosts,
        const URLPatternSet& default_runtime_allowed_hosts) {
  NetworkPermissionsUpdateHelper* helper = new NetworkPermissionsUpdateHelper(
      browser_context,
      base::BindOnce(
          &PermissionsUpdater::NotifyDefaultPolicyHostRestrictionsUpdated,
          browser_context, default_runtime_blocked_hosts.Clone(),
          default_runtime_allowed_hosts.Clone()));

  NetworkPermissionsUpdater::UpdateAllExtensions(
      *browser_context,
      base::BindOnce(&NetworkPermissionsUpdateHelper::OnOriginAccessUpdated,
                     helper->weak_factory_.GetWeakPtr()));
}

PermissionsUpdater::NetworkPermissionsUpdateHelper::
    NetworkPermissionsUpdateHelper(content::BrowserContext* browser_context,
                                   base::OnceClosure dispatch_event)
    : dispatch_event_(std::move(dispatch_event)),
      shutdown_subscription_(
          PermissionsUpdaterShutdownNotifierFactory::GetInstance()
              ->Get(browser_context)
              ->Subscribe(base::BindRepeating(
                  &NetworkPermissionsUpdateHelper::OnShutdown,
                  base::Unretained(this)))) {}

PermissionsUpdater::NetworkPermissionsUpdateHelper::
    ~NetworkPermissionsUpdateHelper() {}

void PermissionsUpdater::NetworkPermissionsUpdateHelper::OnShutdown() {
  // The profile is shutting down. Don't dispatch the permissions updated
  // event, and clean up the dangling references.
  delete this;
}

void PermissionsUpdater::NetworkPermissionsUpdateHelper::
    OnOriginAccessUpdated() {
  // The origin access list was successfully updated; dispatch the event
  // and clean up dangling references.
  std::move(dispatch_event_).Run();
  delete this;
}

PermissionsUpdater::PermissionsUpdater(content::BrowserContext* browser_context)
    : PermissionsUpdater(browser_context, INIT_FLAG_NONE) {}

PermissionsUpdater::PermissionsUpdater(content::BrowserContext* browser_context,
                                       InitFlag init_flag)
    : browser_context_(browser_context), init_flag_(init_flag) {}

PermissionsUpdater::~PermissionsUpdater() {}

void PermissionsUpdater::GrantOptionalPermissions(
    const Extension& extension,
    const PermissionSet& permissions,
    base::OnceClosure completion_callback) {
  CHECK(PermissionsParser::GetOptionalPermissions(&extension)
            .Contains(permissions))
      << "Cannot add optional permissions that are not "
      << "specified in the manifest.";

  // Granted optional permissions are stored in both the granted permissions (so
  // we don't later disable the extension when we check the active permissions
  // against the granted set to determine if there's a permissions increase) and
  // the granted runtime permissions (so they don't get withheld with runtime
  // host permissions enabled). They're also added to the active set, which is
  // the permission set stored in preferences representing the extension's
  // currently-desired permission state.
  // TODO(tjudkins): The reasoning for this doesn't entirely hold true now that
  // we check both the granted permissions and runtime permissions to detect a
  // permission increase. We should address this as we continue working on
  // reducing the different ways we store permissions into a unified concept.
  constexpr int permissions_store_mask =
      kActivePermissions | kGrantedPermissions | kRuntimeGrantedPermissions;
  AddPermissionsImpl(extension, permissions, permissions_store_mask,
                     permissions, std::move(completion_callback));
}

void PermissionsUpdater::GrantRuntimePermissions(
    const Extension& extension,
    const PermissionSet& permissions,
    base::OnceClosure completion_callback) {
  // We don't want to grant the extension object/process more privilege than it
  // requested, even if the user grants additional permission. For instance, if
  // the extension requests https://maps.google.com and the user grants
  // https://*.google.com, we only want to grant the extension itself
  // https://maps.google.com. Since we updated the prefs with the exact
  // granted permissions (*.google.com), if the extension later requests
  // increased permissions that are already covered, they will be auto-granted.

  // Determine which permissions to add to the extension.
  const PermissionSet& withheld =
      extension.permissions_data()->withheld_permissions();

  // We add the intersection of any permissions that were withheld and the
  // permissions that were granted. Since these might not be directly
  // overlapping, we need to use a detailed intersection behavior here.
  std::unique_ptr<const PermissionSet> active_permissions_to_add =
      PermissionSet::CreateIntersection(
          withheld, permissions,
          URLPatternSet::IntersectionBehavior::kDetailed);
  CHECK(extension.permissions_data()->withheld_permissions().Contains(
      *active_permissions_to_add))
      << "Cannot add runtime granted permissions that were not withheld.";

  // Adding runtime granted permissions does not add permissions to the
  // granted or active permissions store, so that behavior taken with the
  // runtime host permissions feature is confined to when the experiment is
  // enabled.
  constexpr int permissions_store_mask = kRuntimeGrantedPermissions;
  AddPermissionsImpl(extension, *active_permissions_to_add,
                     permissions_store_mask, permissions,
                     std::move(completion_callback));
}

void PermissionsUpdater::RevokeOptionalPermissions(
    const Extension& extension,
    const PermissionSet& permissions,
    RemoveType remove_type,
    base::OnceClosure completion_callback) {
  CHECK(PermissionsParser::GetOptionalPermissions(&extension)
            .Contains(permissions))
      << "Cannot remove optional permissions that are not "
      << "specified in the manifest.";

  // Revoked optional permissions are removed from granted and runtime-granted
  // permissions only if the user, and not the extension, removed them (i.e.,
  // `remove_type` == REMOVE_HARD). This allows the extension to add them again
  // without prompting the user. They are always removed from the active set,
  // which is the set of permissions the extension currently requests.
  int permissions_store_mask = kActivePermissions;
  if (remove_type == REMOVE_HARD) {
    permissions_store_mask |= kGrantedPermissions | kRuntimeGrantedPermissions;

    // We don't allow the hard-removal of user-permitted sites on a per-
    // extension basis. Instead, these permissions must be removed by removing
    // the user-permitted site entry. If this changes, we'll need to adjust
    // this to add back these sites, as we do in RevokeRuntimePermissions().
#if DCHECK_IS_ON()
    URLPatternSet user_permitted_sites =
        GetUserPermittedPatternSet(*browser_context_);
    PermissionSet user_permitted_set(
        APIPermissionSet(), ManifestPermissionSet(),
        user_permitted_sites.Clone(), user_permitted_sites.Clone());
    std::unique_ptr<const PermissionSet> user_permitted_being_removed =
        PermissionSet::CreateIntersection(
            permissions, user_permitted_set,
            URLPatternSet::IntersectionBehavior::kDetailed);
    DCHECK(user_permitted_being_removed->effective_hosts().is_empty())
        << "Attempting to hard-remove optional permission to user-permitted "
           "sites: "
        << user_permitted_being_removed->effective_hosts();
#endif
  }

  // Revoking optional permissions is usually done by the extension, so we allow
  // revoking user-permitted sites (the extension can opt-out of having
  // permissions). So in this case, the new active permissions are simply the
  // current active minus any revoked permissions.
  std::unique_ptr<const PermissionSet> new_active_permissions =
      PermissionSet::CreateDifference(
          extension.permissions_data()->active_permissions(), permissions);

  // Since this adjusts the active(desired) permissions and is usually done by
  // the extension, we need not withhold the optional permissions.
  constexpr bool withhold_optional_permissions = false;

  RemovePermissionsImpl(extension, std::move(new_active_permissions),
                        permissions, permissions_store_mask,
                        withhold_optional_permissions,
                        std::move(completion_callback));
}

void PermissionsUpdater::RevokeRuntimePermissions(
    const Extension& extension,
    const PermissionSet& permissions,
    base::OnceClosure completion_callback) {
  // Similar to the process in adding permissions, we might be revoking more
  // permissions than the extension currently has explicit access to. For
  // instance, we might be revoking https://*.google.com/* even if the extension
  // only has https://maps.google.com/*.
  const PermissionSet& active =
      extension.permissions_data()->active_permissions();

  // Unlike adding permissions, we should know that any permissions we remove
  // are a subset of the permissions the extension has active (because we only
  // allow removal origins and the extension can't have a broader origin than
  // what it has granted). Because of this, we can just look for any patterns
  // contained in both sets.
  std::unique_ptr<const PermissionSet> active_permissions_to_remove =
      PermissionSet::CreateIntersection(
          active, permissions,
          URLPatternSet::IntersectionBehavior::kPatternsContainedByBoth);

  CHECK(active.Contains(*active_permissions_to_remove))
      << "Cannot remove permissions that are not active.";
  CHECK(GetRevokablePermissions(&extension)->Contains(permissions))
      << "Cannot remove non-revokable permissions.";

  // Calculate a set of permissions to keep active on the extension, even if
  // they were included in the removal set. This includes chrome://favicon
  // (which would be included in `active_permissions_to_remove` if the removal
  // set is <all_urls>) and any sites the user indicated all extensions may
  // always run on.
  std::unique_ptr<const PermissionSet> permissions_to_keep;
  {
    URLPatternSet explicit_hosts;
    URLPatternSet scriptable_hosts;

    // Don't allow removing chrome://favicon, if it was previously granted.
    for (const auto& pattern : active_permissions_to_remove->explicit_hosts()) {
      bool is_chrome_favicon = pattern.scheme() == content::kChromeUIScheme &&
                               pattern.host() == chrome::kChromeUIFaviconHost;
      if (is_chrome_favicon) {
        explicit_hosts.AddPattern(pattern);
        break;
      }
    }

    // If the corresponding feature is enabled, add in user-permitted sites.
    if (base::FeatureList::IsEnabled(
            extensions_features::kExtensionsMenuAccessControl)) {
      URLPatternSet always_permitted_set =
          GetUserPermittedPatternSet(*browser_context_);
      explicit_hosts.AddPatterns(always_permitted_set);
      scriptable_hosts.AddPatterns(always_permitted_set);
    }

    PermissionSet permitted_set(APIPermissionSet(), ManifestPermissionSet(),
                                std::move(explicit_hosts),
                                std::move(scriptable_hosts));

    permissions_to_keep = PermissionSet::CreateIntersection(
        *active_permissions_to_remove, permitted_set,
        URLPatternSet::IntersectionBehavior::kDetailed);
  }

  // Calculate the new set of active permissions. This is the current
  // permissions minus the permissions to remove, but then adding back in any
  // of the permissions we've explicitly identified as those we should keep.
  std::unique_ptr<const PermissionSet> new_active_permissions =
      PermissionSet::CreateDifference(active, *active_permissions_to_remove);
  new_active_permissions =
      PermissionSet::CreateUnion(*new_active_permissions, *permissions_to_keep);

  // Runtime permissions have a separate store in prefs.
  // Note that we remove all the permissions in `permissions` from
  // runtime-granted permissions. User-permitted sites are granted
  // separately, and not considered runtime-granted permissions. This ensures
  // that when a user changes a site from permitted to non-permitted or vice
  // versa, and extension's specific stored permissions are unaffected.
  constexpr int permissions_store_mask = kRuntimeGrantedPermissions;
  RemovePermissionsImpl(extension, std::move(new_active_permissions),
                        permissions, permissions_store_mask,
                        /*withhold_optional_permissions=*/true,
                        std::move(completion_callback));
}

void PermissionsUpdater::ApplyPolicyHostRestrictions(
    const Extension& extension) {
  ExtensionManagement* management =
      ExtensionManagementFactory::GetForBrowserContext(browser_context_);
  if (management->UsesDefaultPolicyHostRestrictions(&extension)) {
    SetUsesDefaultHostRestrictions(&extension);
  } else {
    SetPolicyHostRestrictions(&extension,
                              management->GetPolicyBlockedHosts(&extension),
                              management->GetPolicyAllowedHosts(&extension));
  }
}

void PermissionsUpdater::SetPolicyHostRestrictions(
    const Extension* extension,
    const URLPatternSet& runtime_blocked_hosts,
    const URLPatternSet& runtime_allowed_hosts) {
  extension->permissions_data()->SetPolicyHostRestrictions(
      runtime_blocked_hosts, runtime_allowed_hosts);

  // Update the BrowserContext origin lists, and send notification to the
  // currently running renderers of the runtime block hosts settings.
  NetworkPermissionsUpdateHelper::UpdatePermissions(
      browser_context_, POLICY, extension, PermissionSet(), base::DoNothing());
}

void PermissionsUpdater::SetUsesDefaultHostRestrictions(
    const Extension* extension) {
  extension->permissions_data()->SetUsesDefaultHostRestrictions();
  NetworkPermissionsUpdateHelper::UpdatePermissions(
      browser_context_, POLICY, extension, PermissionSet(), base::DoNothing());
}

void PermissionsUpdater::SetDefaultPolicyHostRestrictions(
    const URLPatternSet& default_runtime_blocked_hosts,
    const URLPatternSet& default_runtime_allowed_hosts) {
  DCHECK_EQ(0, init_flag_ & INIT_FLAG_TRANSIENT);

  PermissionsData::SetDefaultPolicyHostRestrictions(
      util::GetBrowserContextId(browser_context_),
      default_runtime_blocked_hosts, default_runtime_allowed_hosts);

  // Update the BrowserContext origin lists, and send notification to the
  // currently running renderers of the runtime block hosts settings.
  NetworkPermissionsUpdateHelper::UpdateDefaultPolicyHostRestrictions(
      browser_context_, default_runtime_blocked_hosts,
      default_runtime_allowed_hosts);
}

void PermissionsUpdater::RemovePermissionsUnsafe(
    const Extension* extension,
    const PermissionSet& to_remove) {
  const PermissionSet& active =
      extension->permissions_data()->active_permissions();
  std::unique_ptr<const PermissionSet> total =
      PermissionSet::CreateDifference(active, to_remove);
  // |successfully_removed| might not equal |to_remove| if |to_remove| contains
  // permissions the extension didn't have.
  std::unique_ptr<const PermissionSet> successfully_removed =
      PermissionSet::CreateDifference(active, *total);

  // TODO(devlin): This seems wrong. Since these permissions are being removed
  // by enterprise policy, we should not update the active permissions set in
  // preferences. That way, if the enterprise policy is changed, the removed
  // permissions would be re-added.
  ExtensionPrefs::Get(browser_context_)
      ->SetDesiredActivePermissions(extension->id(), *total);

  SetPermissions(extension, std::move(total),
                 /*withhold_optional_permissions=*/true);
  NetworkPermissionsUpdateHelper::UpdatePermissions(
      browser_context_, REMOVED, extension, *successfully_removed,
      base::DoNothing());
}

std::unique_ptr<const PermissionSet>
PermissionsUpdater::GetRevokablePermissions(const Extension* extension) const {
  // Any permissions not required by the extension are revokable.
  const PermissionSet& required =
      PermissionsParser::GetRequiredPermissions(extension);
  std::unique_ptr<const PermissionSet> revokable_permissions =
      PermissionSet::CreateDifference(
          extension->permissions_data()->active_permissions(), required);

  // Additionally, some required permissions may be revokable if they can be
  // withheld by the ScriptingPermissionsModifier.
  std::unique_ptr<const PermissionSet> revokable_scripting_permissions =
      PermissionsManager::Get(browser_context_)
          ->GetRevokablePermissions(*extension);

  if (revokable_scripting_permissions) {
    revokable_permissions = PermissionSet::CreateUnion(
        *revokable_permissions, *revokable_scripting_permissions);
  }
  return revokable_permissions;
}

void PermissionsUpdater::GrantActivePermissions(const Extension* extension) {
  CHECK(extension);

  ExtensionPrefs::Get(browser_context_)
      ->AddGrantedPermissions(
          extension->id(), extension->permissions_data()->active_permissions());
}

void PermissionsUpdater::InitializePermissions(const Extension* extension) {
  std::unique_ptr<const PermissionSet> desired_permissions_wrapper;
  const PermissionSet* desired_permissions = nullptr;

  PermissionsManager* permissions_manager =
      PermissionsManager::Get(browser_context_);
  DCHECK(permissions_manager);

  // If |extension| is a transient dummy extension, we do not want to look for
  // it in preferences.
  if (init_flag_ & INIT_FLAG_TRANSIENT) {
    desired_permissions = &extension->permissions_data()->active_permissions();
  } else {
    desired_permissions_wrapper =
        permissions_manager->GetBoundedExtensionDesiredPermissions(*extension);
    desired_permissions = desired_permissions_wrapper.get();
  }

  std::unique_ptr<const PermissionSet> granted_permissions =
      permissions_manager->GetEffectivePermissionsToGrant(*extension,
                                                          *desired_permissions);

  if ((init_flag_ & INIT_FLAG_TRANSIENT) == 0) {
    // Set the desired permissions in prefs.
    // - For new installs, this initializes the desired active permissions.
    // - For updates, this ensures the desired active permissions contain any
    //   newly-added permissions and removes any no-longer-requested
    //   permissions.
    // - For pref corruption, this resets the prefs to a sane state.
    // - This also resets prefs from https://crbug.com/1343643, in which
    //   desired active permissions may not have included all required
    //   permissions.
    ExtensionPrefs::Get(browser_context_)
        ->SetDesiredActivePermissions(extension->id(), *desired_permissions);

    extension->permissions_data()->SetContextId(
        util::GetBrowserContextId(browser_context_));

    // Apply per-extension policy if set.
    ApplyPolicyHostRestrictions(*extension);
  }

  SetPermissions(extension, std::move(granted_permissions),
                 /*withhold_optional_permissions=*/true);
}

void PermissionsUpdater::AddPermissionsForTesting(
    const Extension& extension,
    const PermissionSet& permissions) {
  AddPermissionsImpl(extension, permissions, kNone, permissions,
                     base::DoNothing());
}

void PermissionsUpdater::SetPermissions(
    const Extension* extension,
    std::unique_ptr<const PermissionSet> new_active,
    bool withhold_optional_permissions) {
  // Calculate the withheld permissions as any permissions that were required
  // or were active and granted via the Permissions API, but are not in the
  // active set.
  const PermissionSet& required =
      PermissionsParser::GetRequiredPermissions(extension);
  const PermissionSet& optional =
      PermissionsParser::GetOptionalPermissions(extension);
  std::unique_ptr<const PermissionSet> desired_permissions =
      PermissionsManager::Get(browser_context_)
          ->GetDesiredActivePermissionsFromPrefs(*extension);
  bool has_optional_permissions = !optional.IsEmpty();
  // TODO(crbug.com/41405109): Currently, withheld permissions should only
  // contain permissions withheld by the runtime host permissions feature.
  // However, there could possibly be API permissions that were removed from the
  // active set by enterprise policy. These shouldn't go in the withheld
  // permission set, since withheld permissions are generally supposed to be
  // grantable. Currently, we can deal with this because all permissions
  // withheld by runtime host permissions are explicit or scriptable hosts, and
  // all permissions blocked by enterprise are API permissions. So to get the
  // set of runtime-hosts-withheld permissions, we just look at the delta in the
  // URLPatternSets. However, this is very fragile, and should be dealt with
  // more robustly.
  URLPatternSet desired_explicit_hosts;
  if (has_optional_permissions && desired_permissions &&
      withhold_optional_permissions) {
    // We need to consider withholding desired optional hosts when the user
    // chooses to revoke an optional permission by updating site access.
    std::unique_ptr<const PermissionSet> desired_optional_permissions =
        PermissionSet::CreateIntersection(*desired_permissions, optional);
    desired_explicit_hosts = URLPatternSet::CreateUnion(
        desired_optional_permissions->explicit_hosts().Clone(),
        required.explicit_hosts().Clone());
  } else {
    desired_explicit_hosts = required.explicit_hosts().Clone();
  }
  URLPatternSet scriptable_hosts = required.scriptable_hosts().Clone();
  std::unique_ptr<const PermissionSet> new_withheld =
      PermissionSet::CreateDifference(
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        std::move(desired_explicit_hosts),
                        std::move(scriptable_hosts)),
          *new_active);

  extension->permissions_data()->SetPermissions(std::move(new_active),
                                                std::move(new_withheld));
}

// static
void PermissionsUpdater::NotifyPermissionsUpdated(
    content::BrowserContext* browser_context,
    EventType event_type,
    scoped_refptr<const Extension> extension,
    std::unique_ptr<const PermissionSet> changed,
    base::OnceClosure completion_callback) {
  if ((changed->IsEmpty() && event_type != POLICY) ||
      browser_context->ShutdownStarted()) {
    std::move(completion_callback).Run();
    return;
  }

  PermissionsManager::UpdateReason reason;
  events::HistogramValue histogram_value = events::UNKNOWN;
  const char* event_name = nullptr;
  Profile* profile = Profile::FromBrowserContext(browser_context);

  if (event_type == REMOVED) {
    reason = PermissionsManager::UpdateReason::kRemoved;
    histogram_value = events::PERMISSIONS_ON_REMOVED;
    event_name = permissions::OnRemoved::kEventName;
  } else if (event_type == ADDED) {
    reason = PermissionsManager::UpdateReason::kAdded;
    histogram_value = events::PERMISSIONS_ON_ADDED;
    event_name = permissions::OnAdded::kEventName;
  } else {
    DCHECK_EQ(POLICY, event_type);
    reason = PermissionsManager::UpdateReason::kPolicy;
  }

  // Notify other APIs or interested parties.
  PermissionsManager::Get(browser_context)
      ->NotifyExtensionPermissionsUpdated(*extension, *changed, reason);

  // Send the new permissions to the renderers iff extension is enabled.
  // A disabled extension can have its permissions updated by the user in
  // chrome://extensions. Disabled extensions will have their permissions
  // updated in the renderer when they are next loaded.
  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(browser_context);
  if (extension_registry->enabled_extensions().Contains(extension->id())) {
    for (auto host_iterator(RenderProcessHost::AllHostsIterator());
         !host_iterator.IsAtEnd(); host_iterator.Advance()) {
      RenderProcessHost* host = host_iterator.GetCurrentValue();
      if (!host->IsInitializedAndNotDead() ||
          !profile->IsSameOrParent(
              Profile::FromBrowserContext(host->GetBrowserContext()))) {
        continue;
      }

      mojom::Renderer* renderer =
          RendererStartupHelperFactory::GetForBrowserContext(
              host->GetBrowserContext())
              ->GetRenderer(host);
      if (!renderer) {
        continue;
      }

      const PermissionsData* permissions_data = extension->permissions_data();
      renderer->UpdatePermissions(
          extension->id(),
          std::move(*permissions_data->active_permissions().Clone()),
          std::move(*permissions_data->withheld_permissions().Clone()),
          permissions_data->policy_blocked_hosts(),
          permissions_data->policy_allowed_hosts(),
          permissions_data->UsesDefaultPolicyHostRestrictions());

      // Notify ScriptInjectionTracker when host permissions change.
      if (!changed->effective_hosts().is_empty()) {
        ScriptInjectionTracker::DidUpdatePermissionsInRenderer(
            base::PassKey<PermissionsUpdater>(), *extension, *host);
      }
    }
  }

  // Trigger the onAdded and onRemoved events in the extension. We explicitly
  // don't do this for policy-related events.
  EventRouter* event_router =
      event_name ? EventRouter::Get(browser_context) : nullptr;
  if (event_router) {
    base::Value::List event_args;
    std::unique_ptr<api::permissions::Permissions> permissions =
        PackPermissionSet(*changed);
    event_args.Append(permissions->ToValue());
    auto event = std::make_unique<Event>(
        histogram_value, event_name, std::move(event_args), browser_context);
    event_router->DispatchEventToExtension(extension->id(), std::move(event));
  }

  std::move(completion_callback).Run();
}

// static
void PermissionsUpdater::NotifyDefaultPolicyHostRestrictionsUpdated(
    content::BrowserContext* browser_context,
    const URLPatternSet default_runtime_blocked_hosts,
    const URLPatternSet default_runtime_allowed_hosts) {
  Profile* profile = Profile::FromBrowserContext(browser_context);

  // Send the new policy to the renderers.
  for (RenderProcessHost::iterator host_iterator(
           RenderProcessHost::AllHostsIterator());
       !host_iterator.IsAtEnd(); host_iterator.Advance()) {
    RenderProcessHost* host = host_iterator.GetCurrentValue();
    if (host->IsInitializedAndNotDead() &&
        profile->IsSameOrParent(
            Profile::FromBrowserContext(host->GetBrowserContext()))) {
      mojom::Renderer* renderer =
          RendererStartupHelperFactory::GetForBrowserContext(
              host->GetBrowserContext())
              ->GetRenderer(host);
      if (renderer) {
        renderer->UpdateDefaultPolicyHostRestrictions(
            default_runtime_blocked_hosts.Clone(),
            default_runtime_allowed_hosts.Clone());
      }
    }
  }
}

void PermissionsUpdater::AddPermissionsImpl(
    const Extension& extension,
    const PermissionSet& active_permissions_to_add,
    int prefs_permissions_store_mask,
    const PermissionSet& permissions_to_add_to_prefs,
    base::OnceClosure completion_callback) {
  std::unique_ptr<const PermissionSet> new_active = PermissionSet::CreateUnion(
      active_permissions_to_add,
      extension.permissions_data()->active_permissions());

  SetPermissions(&extension, std::move(new_active),
                 /*withhold_optional_permissions=*/true);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
  if ((prefs_permissions_store_mask & kActivePermissions) != 0) {
    prefs->AddDesiredActivePermissions(extension.id(),
                                       permissions_to_add_to_prefs);
  }

  if ((prefs_permissions_store_mask & kGrantedPermissions) != 0) {
    prefs->AddGrantedPermissions(extension.id(), permissions_to_add_to_prefs);
  }

  if ((prefs_permissions_store_mask & kRuntimeGrantedPermissions) != 0) {
    prefs->AddRuntimeGrantedPermissions(extension.id(),
                                        permissions_to_add_to_prefs);
  }

  NetworkPermissionsUpdateHelper::UpdatePermissions(
      browser_context_, ADDED, &extension, active_permissions_to_add,
      std::move(completion_callback));
}

void PermissionsUpdater::RemovePermissionsImpl(
    const Extension& extension,
    std::unique_ptr<const PermissionSet> new_active_permissions,
    const PermissionSet& permissions_to_remove_from_prefs,
    int prefs_permissions_store_mask,
    bool withhold_optional_permissions,
    base::OnceClosure completion_callback) {
  SetPermissions(&extension, std::move(new_active_permissions),
                 withhold_optional_permissions);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
  if ((prefs_permissions_store_mask & kActivePermissions) != 0) {
    prefs->RemoveDesiredActivePermissions(extension.id(),
                                          permissions_to_remove_from_prefs);
  }

  // NOTE: Currently, this code path is only reached in unit tests. See comment
  // above REMOVE_HARD in the header file.
  if ((prefs_permissions_store_mask & kGrantedPermissions) != 0) {
    prefs->RemoveGrantedPermissions(extension.id(),
                                    permissions_to_remove_from_prefs);
  }

  if ((prefs_permissions_store_mask & kRuntimeGrantedPermissions) != 0) {
    prefs->RemoveRuntimeGrantedPermissions(extension.id(),
                                           permissions_to_remove_from_prefs);
  }

  // For the notification, we consider the changed set to be the set of
  // permissions to remove from preferences, rather than the new active
  // permissions (which can include things like user-permitted sites).
  NetworkPermissionsUpdateHelper::UpdatePermissions(
      browser_context_, REMOVED, &extension, permissions_to_remove_from_prefs,
      std::move(completion_callback));
}

// static
void PermissionsUpdater::EnsureAssociatedFactoryBuilt() {
  PermissionsUpdaterShutdownNotifierFactory::GetInstance();
}

}  // namespace extensions
