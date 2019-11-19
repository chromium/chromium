// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions_updater.h"

#include <set>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/permissions/permissions_api_helpers.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/permissions.h"
#include "chrome/common/webui_url_constants.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/cors_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

using content::RenderProcessHost;
using extensions::permissions_api_helpers::PackPermissionSet;

namespace extensions {

namespace permissions = api::permissions;

namespace {

// Returns a PermissionSet that has the active permissions of the extension,
// bounded to its current manifest.
std::unique_ptr<const PermissionSet> GetBoundedActivePermissions(
    const Extension* extension,
    const PermissionSet* active_permissions) {
  // If the extension has used the optional permissions API, it will have a
  // custom set of active permissions defined in the extension prefs. Here,
  // we update the extension's active permissions based on the prefs.
  if (!active_permissions)
    return extension->permissions_data()->active_permissions().Clone();

  const PermissionSet& required_permissions =
      PermissionsParser::GetRequiredPermissions(extension);

  // We restrict the active permissions to be within the bounds defined in the
  // extension's manifest.
  //  a) active permissions must be a subset of optional + default permissions
  //  b) active permissions must contains all default permissions
  std::unique_ptr<const PermissionSet> total_permissions =
      PermissionSet::CreateUnion(
          required_permissions,
          PermissionsParser::GetOptionalPermissions(extension));

  // Make sure the active permissions contain no more than optional + default.
  std::unique_ptr<const PermissionSet> adjusted_active =
      PermissionSet::CreateIntersection(*total_permissions,
                                        *active_permissions);

  // Make sure the active permissions contain the default permissions.
  adjusted_active =
      PermissionSet::CreateUnion(required_permissions, *adjusted_active);

  return adjusted_active;
}

std::unique_ptr<PermissionsUpdater::Delegate>& GetDelegateWrapper() {
  static base::NoDestructor<std::unique_ptr<PermissionsUpdater::Delegate>>
      delegate_wrapper;
  return *delegate_wrapper;
}

PermissionsUpdater::Delegate* GetDelegate() {
  return GetDelegateWrapper().get();
}

// A helper class to watch profile lifetime.
class PermissionsUpdaterShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
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

  DISALLOW_COPY_AND_ASSIGN(PermissionsUpdaterShutdownNotifierFactory);
};

}  // namespace

// A helper class to asynchronously dispatch the event to notify policy host
// restrictions or permissions once they have been updated. This will fire the
// event if and only if the BrowserContext is still valid.
// This class manages its own lifetime and deletes itself when either the
// permissions updated event is fired, or the BrowserContext is shut down
// (whichever happens first).
class PermissionsUpdater::NetworkPermissionsUpdateHelper {
 public:
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
  std::unique_ptr<KeyedServiceShutdownNotifier::Subscription>
      shutdown_subscription_;
  base::WeakPtrFactory<NetworkPermissionsUpdateHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkPermissionsUpdateHelper);
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

  std::vector<network::mojom::CorsOriginPatternPtr> allow_list =
      CreateCorsOriginAccessAllowList(
          *extension,
          PermissionsData::EffectiveHostPermissionsMode::kOmitTabSpecific);

  NetworkPermissionsUpdateHelper* helper = new NetworkPermissionsUpdateHelper(
      browser_context,
      base::BindOnce(&PermissionsUpdater::NotifyPermissionsUpdated,
                     browser_context, event_type, extension,
                     changed.Clone(), std::move(completion_callback)));

  // After an asynchronous call below, the helper will call
  // NotifyPermissionsUpdated if the profile is still valid.
  browser_context->SetCorsOriginAccessListForOrigin(
      url::Origin::Create(extension->url()), std::move(allow_list),
      CreateCorsOriginAccessBlockList(*extension),
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

  const ExtensionSet& extensions =
      ExtensionRegistry::Get(browser_context)->enabled_extensions();
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      extensions.size(),
      base::BindOnce(&NetworkPermissionsUpdateHelper::OnOriginAccessUpdated,
                     helper->weak_factory_.GetWeakPtr()));

  for (const auto& extension : extensions) {
    std::vector<network::mojom::CorsOriginPatternPtr> allow_list =
        CreateCorsOriginAccessAllowList(
            *extension,
            PermissionsData::EffectiveHostPermissionsMode::kOmitTabSpecific);
    browser_context->SetCorsOriginAccessListForOrigin(
        url::Origin::Create(extension->url()), std::move(allow_list),
        CreateCorsOriginAccessBlockList(*extension), barrier_closure);
  }
}

PermissionsUpdater::NetworkPermissionsUpdateHelper::
    NetworkPermissionsUpdateHelper(content::BrowserContext* browser_context,
                                   base::OnceClosure dispatch_event)
    : dispatch_event_(std::move(dispatch_event)),
      shutdown_subscription_(
          PermissionsUpdaterShutdownNotifierFactory::GetInstance()
              ->Get(browser_context)
              ->Subscribe(
                  base::Bind(&NetworkPermissionsUpdateHelper::OnShutdown,
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

// static
void PermissionsUpdater::SetPlatformDelegate(
    std::unique_ptr<Delegate> delegate) {
  GetDelegateWrapper() = std::move(delegate);
}

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
  // TODO(devlin): Ideally, we'd have this CHECK in place, but unit tests are
  // currently violating it.
  CHECK(PermissionsParser::GetOptionalPermissions(&extension)
            .Contains(permissions))
      << "Cannot remove optional permissions that are not "
      << "specified in the manifest.";

  // Revoked optional permissions are removed from granted and runtime-granted
  // permissions only if the user, and not the extension, removed them. This
  // allows the extension to add them again without prompting the user. They are
  // always removed from the active set, which is the set of permissions the
  // the extension currently requests.
  int permissions_store_mask = kActivePermissions;
  if (remove_type == REMOVE_HARD)
    permissions_store_mask |= kGrantedPermissions | kRuntimeGrantedPermissions;

  RemovePermissionsImpl(extension, permissions, permissions_store_mask,
                        permissions, std::move(completion_callback));
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
  // are a superset of the permissions the extension has active (because we only
  // allow removal origins and the extension can't have a broader origin than
  // what it has granted).
  std::unique_ptr<const PermissionSet> active_permissions_to_remove =
      PermissionSet::CreateIntersection(
          active, permissions,
          URLPatternSet::IntersectionBehavior::kPatternsContainedByBoth);
  // One exception: If we're revoking a permission like "<all_urls>", we need
  // to make sure it doesn't revoke the included chrome://favicon permission.
  std::set<URLPattern> removable_explicit_hosts;
  bool needs_adjustment = false;
  for (const auto& pattern : active_permissions_to_remove->explicit_hosts()) {
    bool is_chrome_favicon = pattern.scheme() == content::kChromeUIScheme &&
                             pattern.host() == chrome::kChromeUIFaviconHost;
    if (is_chrome_favicon)
      needs_adjustment = true;
    else
      removable_explicit_hosts.insert(pattern);
  }
  if (needs_adjustment) {
    // Tedious, because PermissionSets are const. :(
    active_permissions_to_remove = std::make_unique<PermissionSet>(
        active_permissions_to_remove->apis().Clone(),
        active_permissions_to_remove->manifest_permissions().Clone(),
        URLPatternSet(removable_explicit_hosts),
        active_permissions_to_remove->scriptable_hosts().Clone());
  }

  CHECK(extension.permissions_data()->active_permissions().Contains(
      *active_permissions_to_remove))
      << "Cannot remove permissions that are not active.";
  CHECK(GetRevokablePermissions(&extension)->Contains(permissions))
      << "Cannot remove non-revokable permissions.";

  // Removing runtime-granted permissions does not remove permissions from
  // the granted permissions store. This is done to ensure behavior taken with
  // the runtime host permissions feature is confined to when the experiment is
  // enabled. Similarly, since the runtime-granted permissions were never added
  // to the active permissions stored in prefs, they are also not removed.
  constexpr int permissions_store_mask = kRuntimeGrantedPermissions;
  RemovePermissionsImpl(extension, *active_permissions_to_remove,
                        permissions_store_mask, permissions,
                        std::move(completion_callback));
}

void PermissionsUpdater::SetPolicyHostRestrictions(
    const Extension* extension,
    const URLPatternSet& runtime_blocked_hosts,
    const URLPatternSet& runtime_allowed_hosts) {
  extension->permissions_data()->SetPolicyHostRestrictions(
      runtime_blocked_hosts, runtime_allowed_hosts);

  // Update the BrowserContext origin lists, and send notification to the
  // currently running renderers of the runtime block hosts settings.
  NetworkPermissionsUpdateHelper::UpdatePermissions(browser_context_, POLICY,
                                                    extension, PermissionSet(),
                                                    base::DoNothing::Once());
}

void PermissionsUpdater::SetUsesDefaultHostRestrictions(
    const Extension* extension) {
  extension->permissions_data()->SetUsesDefaultHostRestrictions();
  NetworkPermissionsUpdateHelper::UpdatePermissions(browser_context_, POLICY,
                                                    extension, PermissionSet(),
                                                    base::DoNothing::Once());
}

void PermissionsUpdater::SetDefaultPolicyHostRestrictions(
    const URLPatternSet& default_runtime_blocked_hosts,
    const URLPatternSet& default_runtime_allowed_hosts) {
  DCHECK_EQ(0, init_flag_ & INIT_FLAG_TRANSIENT);

  PermissionsData::SetDefaultPolicyHostRestrictions(
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
  constexpr bool update_active_prefs = true;
  SetPermissions(extension, std::move(total), update_active_prefs);
  NetworkPermissionsUpdateHelper::UpdatePermissions(
      browser_context_, REMOVED, extension, *successfully_removed,
      base::DoNothing::Once());
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
      ScriptingPermissionsModifier(browser_context_,
                                   base::WrapRefCounted(extension))
          .GetRevokablePermissions();

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
  std::unique_ptr<const PermissionSet> bounded_wrapper;
  const PermissionSet* bounded_active = nullptr;
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
  // If |extension| is a transient dummy extension, we do not want to look for
  // it in preferences.
  if (init_flag_ & INIT_FLAG_TRANSIENT) {
    bounded_active = &extension->permissions_data()->active_permissions();
  } else {
    std::unique_ptr<const PermissionSet> active_permissions =
        prefs->GetActivePermissions(extension->id());
    bounded_wrapper =
        GetBoundedActivePermissions(extension, active_permissions.get());
    bounded_active = bounded_wrapper.get();
  }

  std::unique_ptr<const PermissionSet> granted_permissions =
      ScriptingPermissionsModifier::WithholdPermissionsIfNecessary(
          *extension, *prefs, *bounded_active);

  if (GetDelegate())
    GetDelegate()->InitializePermissions(extension, &granted_permissions);

  bool update_active_permissions = false;
  if ((init_flag_ & INIT_FLAG_TRANSIENT) == 0) {
    update_active_permissions = true;
    // Apply per-extension policy if set.
    ExtensionManagement* management =
        ExtensionManagementFactory::GetForBrowserContext(browser_context_);
    if (!management->UsesDefaultPolicyHostRestrictions(extension)) {
      SetPolicyHostRestrictions(extension,
                                management->GetPolicyBlockedHosts(extension),
                                management->GetPolicyAllowedHosts(extension));
    }
  }

  SetPermissions(extension, std::move(granted_permissions),
                 update_active_permissions);
}

void PermissionsUpdater::AddPermissionsForTesting(
    const Extension& extension,
    const PermissionSet& permissions) {
  AddPermissionsImpl(extension, permissions, kNone, permissions,
                     base::DoNothing::Once());
}

void PermissionsUpdater::SetPermissions(
    const Extension* extension,
    std::unique_ptr<const PermissionSet> new_active,
    bool update_prefs) {
  // Calculate the withheld permissions as any permissions that were required,
  // but are not in the active set.
  const PermissionSet& required =
      PermissionsParser::GetRequiredPermissions(extension);
  // TODO(https://crbug.com/869403): Currently, withheld permissions should only
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
  std::unique_ptr<const PermissionSet> new_withheld =
      PermissionSet::CreateDifference(
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        required.explicit_hosts().Clone(),
                        required.scriptable_hosts().Clone()),
          *new_active);

  extension->permissions_data()->SetPermissions(std::move(new_active),
                                                std::move(new_withheld));

  if (update_prefs) {
    ExtensionPrefs::Get(browser_context_)
        ->SetActivePermissions(
            extension->id(),
            extension->permissions_data()->active_permissions());
  }
}

// static
void PermissionsUpdater::NotifyPermissionsUpdated(
    content::BrowserContext* browser_context,
    EventType event_type,
    scoped_refptr<const Extension> extension,
    std::unique_ptr<const PermissionSet> changed,
    base::OnceClosure completion_callback) {
  if (changed->IsEmpty() && event_type != POLICY) {
    std::move(completion_callback).Run();
    return;
  }

  UpdatedExtensionPermissionsInfo::Reason reason;
  events::HistogramValue histogram_value = events::UNKNOWN;
  const char* event_name = NULL;
  Profile* profile = Profile::FromBrowserContext(browser_context);

  if (event_type == REMOVED) {
    reason = UpdatedExtensionPermissionsInfo::REMOVED;
    histogram_value = events::PERMISSIONS_ON_REMOVED;
    event_name = permissions::OnRemoved::kEventName;
  } else if (event_type == ADDED) {
    reason = UpdatedExtensionPermissionsInfo::ADDED;
    histogram_value = events::PERMISSIONS_ON_ADDED;
    event_name = permissions::OnAdded::kEventName;
  } else {
    DCHECK_EQ(POLICY, event_type);
    reason = UpdatedExtensionPermissionsInfo::POLICY;
  }

  // Notify other APIs or interested parties.
  UpdatedExtensionPermissionsInfo info =
      UpdatedExtensionPermissionsInfo(extension.get(), *changed, reason);
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
      content::Source<Profile>(profile),
      content::Details<UpdatedExtensionPermissionsInfo>(&info));

  ExtensionMsg_UpdatePermissions_Params params;
  params.extension_id = extension->id();
  params.active_permissions = ExtensionMsg_PermissionSetStruct(
      extension->permissions_data()->active_permissions());
  params.withheld_permissions = ExtensionMsg_PermissionSetStruct(
      extension->permissions_data()->withheld_permissions());
  params.uses_default_policy_host_restrictions =
      extension->permissions_data()->UsesDefaultPolicyHostRestrictions();
  if (!params.uses_default_policy_host_restrictions) {
    params.policy_blocked_hosts =
        extension->permissions_data()->policy_blocked_hosts().Clone();
    params.policy_allowed_hosts =
        extension->permissions_data()->policy_allowed_hosts().Clone();
  }

  // Send the new permissions to the renderers.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost* host = i.GetCurrentValue();
    if (profile->IsSameProfile(
            Profile::FromBrowserContext(host->GetBrowserContext()))) {
      host->Send(new ExtensionMsg_UpdatePermissions(params));
    }
  }

  // Trigger the onAdded and onRemoved events in the extension. We explicitly
  // don't do this for policy-related events.
  EventRouter* event_router =
      event_name ? EventRouter::Get(browser_context) : nullptr;
  if (event_router) {
    std::unique_ptr<base::ListValue> value(new base::ListValue());
    std::unique_ptr<api::permissions::Permissions> permissions =
        PackPermissionSet(*changed);
    value->Append(permissions->ToValue());
    auto event = std::make_unique<Event>(histogram_value, event_name,
                                         std::move(value), browser_context);
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

  ExtensionMsg_UpdateDefaultPolicyHostRestrictions_Params params;
  params.default_policy_blocked_hosts = default_runtime_blocked_hosts.Clone();
  params.default_policy_allowed_hosts = default_runtime_allowed_hosts.Clone();

  // Send the new policy to the renderers.
  for (RenderProcessHost::iterator host_iterator(
           RenderProcessHost::AllHostsIterator());
       !host_iterator.IsAtEnd(); host_iterator.Advance()) {
    RenderProcessHost* host = host_iterator.GetCurrentValue();
    if (profile->IsSameProfile(
            Profile::FromBrowserContext(host->GetBrowserContext()))) {
      host->Send(new ExtensionMsg_UpdateDefaultPolicyHostRestrictions(params));
    }
  }
}

void PermissionsUpdater::AddPermissionsImpl(
    const Extension& extension,
    const PermissionSet& active_permissions_to_add,
    int permissions_store_mask,
    const PermissionSet& prefs_permissions_to_add,
    base::OnceClosure completion_callback) {
  std::unique_ptr<const PermissionSet> new_active = PermissionSet::CreateUnion(
      active_permissions_to_add,
      extension.permissions_data()->active_permissions());

  bool update_active_prefs = (permissions_store_mask & kActivePermissions) != 0;
  SetPermissions(&extension, std::move(new_active), update_active_prefs);

  if ((permissions_store_mask & kGrantedPermissions) != 0) {
    // TODO(devlin): Could we only grant |permissions|, rather than all those
    // in the active permissions? In theory, all other active permissions have
    // already been granted.
    GrantActivePermissions(&extension);
  }

  if ((permissions_store_mask & kRuntimeGrantedPermissions) != 0) {
    ExtensionPrefs::Get(browser_context_)
        ->AddRuntimeGrantedPermissions(extension.id(),
                                       prefs_permissions_to_add);
  }

  NetworkPermissionsUpdateHelper::UpdatePermissions(
      browser_context_, ADDED, &extension, active_permissions_to_add,
      std::move(completion_callback));
}

void PermissionsUpdater::RemovePermissionsImpl(
    const Extension& extension,
    const PermissionSet& active_permissions_to_remove,
    int permissions_store_mask,
    const PermissionSet& prefs_permissions_to_remove,
    base::OnceClosure completion_callback) {
  std::unique_ptr<const PermissionSet> new_active =
      PermissionSet::CreateDifference(
          extension.permissions_data()->active_permissions(),
          active_permissions_to_remove);

  bool update_active_prefs = (permissions_store_mask & kActivePermissions) != 0;
  SetPermissions(&extension, std::move(new_active), update_active_prefs);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
  // NOTE: Currently, this code path is only reached in unit tests. See comment
  // above REMOVE_HARD in the header file.
  if ((permissions_store_mask & kGrantedPermissions) != 0) {
    prefs->RemoveGrantedPermissions(extension.id(),
                                    prefs_permissions_to_remove);
  }

  if ((permissions_store_mask & kRuntimeGrantedPermissions) != 0) {
    prefs->RemoveRuntimeGrantedPermissions(extension.id(),
                                           prefs_permissions_to_remove);
  }

  NetworkPermissionsUpdateHelper::UpdatePermissions(
      browser_context_, REMOVED, &extension, active_permissions_to_remove,
      std::move(completion_callback));
}

}  // namespace extensions
