// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMART_CARD_SMART_CARD_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_SMART_CARD_SMART_CARD_PERMISSION_CONTEXT_H_

#include <cstdint>
#include <map>
#include <set>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/smart_card/smart_card_permission_request.h"
#include "chrome/browser/smart_card/smart_card_reader_tracker.h"
#include "components/permissions/object_permission_context_base.h"
#include "content/public/browser/smart_card_delegate.h"
#include "url/origin.h"

class Profile;

namespace content {
class RenderFrameHost;
}  // namespace content
namespace settings {
class SmartCardReaderPermissionsSiteSettingsHandlerTest;
}  // namespace settings
namespace site_settings {
class SiteSettingsHelperChooserExceptionTest;
}  // namespace site_settings

class SmartCardPermissionContext
    : public permissions::ObjectPermissionContextBase,
      public permissions::ObjectPermissionContextBase::PermissionObserver {
 public:
  // Callback type to report whether the user allowed the connection request.
  using RequestReaderPermissionCallback = base::OnceCallback<void(bool)>;

  explicit SmartCardPermissionContext(Profile* profile);
  SmartCardPermissionContext(const SmartCardPermissionContext&) = delete;
  SmartCardPermissionContext& operator=(const SmartCardPermissionContext&) =
      delete;
  ~SmartCardPermissionContext() override;

  bool HasReaderPermission(content::RenderFrameHost& render_frame_host,
                           const std::string& reader_name);

  void RequestReaderPermisssion(content::RenderFrameHost& render_frame_host,
                                const std::string& reader_name,
                                RequestReaderPermissionCallback callback);

  // permissions::ObjectPermissionContextBase:
  std::string GetKeyForObject(const base::Value::Dict& object) override;
  bool IsValidObject(const base::Value::Dict& object) override;
  std::u16string GetObjectDisplayName(const base::Value::Dict& object) override;

  // permissions::ObjectPermissionContextBase::PermissionObserver:
  void OnPermissionRevoked(const url::Origin& origin) override;

  void AddObserver(content::SmartCardDelegate::PermissionObserver* observer);
  void RemoveObserver(content::SmartCardDelegate::PermissionObserver* observer);

  void RevokeEphemeralPermissions();
  void RevokeAllPermissions();
  bool RevokeObjectPermissions(const url::Origin& origin) override;

  // Checks whether |origin|'s value of |guard_content_settings_type_| is both:
  // - set to "allow"
  // - set by policy
  bool IsAllowlistedByPolicy(const url::Origin& origin) const;

  bool CanRequestObjectPermission(const url::Origin& origin) const override;

  // The two methods below are overridden to expose a symbolic "All readers"
  // device in case of allowlisting via policy.
  std::vector<std::unique_ptr<Object>> GetGrantedObjects(
      const url::Origin& origin) override;
  std::vector<std::unique_ptr<Object>> GetAllGrantedObjects() override;

 private:
  friend class SmartCardPermissionContextTest;
  friend class settings::SmartCardReaderPermissionsSiteSettingsHandlerTest;
  friend class PageInfoBubbleViewInteractiveUiTest;
  friend class ChromeOsSmartCardDelegateBrowserTest;
  friend class site_settings::SiteSettingsHelperChooserExceptionTest;

  class OneTimeObserver;
  class PowerSuspendObserver;
  class ReaderObserver;

  bool HasReaderPermission(const url::Origin& origin,
                           const std::string& reader_name);

  // The given permission won't be persisted.
  void GrantEphemeralReaderPermission(const url::Origin& origin,
                                      const std::string& reader_name);

  void GrantPersistentReaderPermission(const url::Origin& origin,
                                       const std::string& reader_name);

  bool HasPersistentReaderPermission(const url::Origin& origin,
                                     const std::string& reader_name);

  void RevokeEphemeralPermissionsForReader(const std::string& reader_name);
  bool RevokeEphemeralPermissionsForOrigin(const url::Origin& origin);

  void OnTrackingStarted(
      std::optional<std::vector<SmartCardReaderTracker::ReaderInfo>>);

  void StopObserving();

  void OnPermissionRequestDecided(const url::Origin& origin,
                                  const std::string& reader_name,
                                  RequestReaderPermissionCallback callback,
                                  PermissionDecision decision);

  SmartCardReaderTracker& GetReaderTracker() const;

  void RevokeEphemeralPermissionIfLongTimeoutOccured(
      const url::Origin& origin,
      const std::string& reader_name);

  // Set of readers to which an origin has ephemeral access to and times the
  // ephemeral permissions should expire.
  base::flat_map<url::Origin, base::flat_map<std::string, base::Time>>
      ephemeral_grants_with_expiry_;

  std::unique_ptr<OneTimeObserver> one_time_observer_;
  std::unique_ptr<PowerSuspendObserver> power_suspend_observer_;
  std::unique_ptr<ReaderObserver> reader_observer_;

  // Instance is owned by this profile.
  base::raw_ref<Profile> profile_;

  base::ObserverList<content::SmartCardDelegate::PermissionObserver>
      permission_observers_;

  base::ScopedObservation<
      permissions::ObjectPermissionContextBase,
      permissions::ObjectPermissionContextBase::PermissionObserver>
      permission_observation_{this};

  base::WeakPtrFactory<SmartCardPermissionContext> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_SMART_CARD_SMART_CARD_PERMISSION_CONTEXT_H_
