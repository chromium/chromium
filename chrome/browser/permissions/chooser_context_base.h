// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_CHOOSER_CONTEXT_BASE_H_
#define CHROME_BROWSER_PERMISSIONS_CHOOSER_CONTEXT_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class HostContentSettingsMap;
class Profile;

namespace url {
class Origin;
}

// This is the base class for services that manage any type of permission that
// is granted through a chooser-style UI instead of a simple allow/deny prompt.
// Subclasses must define the structure of the objects that are stored.
class ChooserContextBase : public KeyedService {
 public:
  struct Object {
    Object(const url::Origin& requesting_origin,
           const base::Optional<url::Origin>& embedding_origin,
           base::Value value,
           content_settings::SettingSource source,
           bool incognito);
    ~Object();

    GURL requesting_origin;
    GURL embedding_origin;
    base::Value value;
    content_settings::SettingSource source;
    bool incognito;
  };

  // This observer can be used to be notified of changes to the permission of a
  // chooser object.
  class PermissionObserver : public base::CheckedObserver {
   public:
    // Notify observers that an object permission changed for the chooser
    // context represented by |guard_content_settings_type| and
    // |data_content_settings_type|.
    virtual void OnChooserObjectPermissionChanged(
        ContentSettingsType guard_content_settings_type,
        ContentSettingsType data_content_settings_type);
    // Notify obsever that an object permission was revoked for
    // |requesting_origin| and |embedding_origin|.
    virtual void OnPermissionRevoked(const url::Origin& requesting_origin,
                                     const url::Origin& embedding_origin);
  };

  void AddObserver(PermissionObserver* observer);
  void RemoveObserver(PermissionObserver* observer);

  ChooserContextBase(Profile* profile,
                     ContentSettingsType guard_content_settings_type,
                     ContentSettingsType data_content_settings_type);
  ~ChooserContextBase() override;

  // Checks whether |requesting_origin| can request permission to access objects
  // when embedded within |embedding_origin|. This is done by checking
  // |guard_content_settings_type_| which will usually be "ask" by default but
  // could be set by the user or group policy.
  bool CanRequestObjectPermission(const url::Origin& requesting_origin,
                                  const url::Origin& embedding_origin);

  // Returns the list of objects that |requesting_origin| has been granted
  // permission to access when embedded within |embedding_origin|.
  //
  // This method may be extended by a subclass to return objects not stored in
  // |host_content_settings_map_|.
  virtual std::vector<std::unique_ptr<Object>> GetGrantedObjects(
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin);

  // Returns the set of all objects that any origin has been granted permission
  // to access.
  //
  // This method may be extended by a subclass to return objects not stored in
  // |host_content_settings_map_|.
  virtual std::vector<std::unique_ptr<Object>> GetAllGrantedObjects();

  // Grants |requesting_origin| access to |object| when embedded within
  // |embedding_origin| by writing it into |host_content_settings_map_|.
  void GrantObjectPermission(const url::Origin& requesting_origin,
                             const url::Origin& embedding_origin,
                             base::Value object);

  // Revokes |requesting_origin|'s permission to access |object| when embedded
  // within |embedding_origin|.
  //
  // This method may be extended by a subclass to revoke permission to access
  // objects returned by GetPreviouslyChosenObjects but not stored in
  // |host_content_settings_map_|.
  virtual void RevokeObjectPermission(const url::Origin& requesting_origin,
                                      const url::Origin& embedding_origin,
                                      const base::Value& object);

  // Validates the structure of an object read from
  // |host_content_settings_map_|.
  virtual bool IsValidObject(const base::Value& object) = 0;

 protected:
  void NotifyPermissionChanged();
  void NotifyPermissionRevoked(const url::Origin& requesting_origin,
                               const url::Origin& embedding_origin);

  const ContentSettingsType guard_content_settings_type_;
  const ContentSettingsType data_content_settings_type_;
  base::ObserverList<PermissionObserver> permission_observer_list_;

 private:
  base::Value GetWebsiteSetting(const url::Origin& requesting_origin,
                                const url::Origin& embedding_origin,
                                content_settings::SettingInfo* info);
  void SetWebsiteSetting(const url::Origin& requesting_origin,
                         const url::Origin& embedding_origin,
                         base::Value value);

  HostContentSettingsMap* const host_content_settings_map_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_CHOOSER_CONTEXT_BASE_H_
