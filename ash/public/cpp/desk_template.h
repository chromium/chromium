// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DESK_TEMPLATE_H_
#define ASH_PUBLIC_CPP_DESK_TEMPLATE_H_

#include <optional>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/app_restore/restore_data.h"
#include "components/sync_device_info/device_info.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Indicates where a desk template originated from.
enum class ASH_PUBLIC_EXPORT DeskTemplateSource {
  // Default value, indicates no value was set.
  kUnknownSource = 0,

  // Desk template created by the user.
  kUser,

  // Desk template pushed through policy.
  kPolicy
};

enum class ASH_PUBLIC_EXPORT DeskTemplateType {
  // Regular desk template.
  kTemplate = 0,

  // Desk saved for Save & Recall.
  kSaveAndRecall,

  // Desk saved for Floating Workspace.
  kFloatingWorkspace,

  // Unknown desk type. This desk is probably created by a later version and
  // should be ignored.
  kUnknown,
};

// Class to represent a desk template. It can be used to create a desk with
// a certain set of application windows specified in |desk_restore_data_|.
class ASH_PUBLIC_EXPORT DeskTemplate {
 public:
  // This constructor is used to instantiate DeskTemplate with a specific
  // source.
  DeskTemplate(base::Uuid uuid,
               DeskTemplateSource source,
               const std::string& name,
               const base::Time created_time,
               DeskTemplateType type);

  // Alternative constructor used if template is defined via policy.
  DeskTemplate(base::Uuid uuid,
               DeskTemplateSource source,
               const std::string& name,
               const base::Time created_time,
               DeskTemplateType type,
               bool should_launch_on_startup,
               base::Value policy);

  DeskTemplate(const DeskTemplate&) = delete;
  DeskTemplate& operator=(const DeskTemplate&) = delete;
  ~DeskTemplate();

  // Returns whether desk templates support the `window`'s app type.
  static bool IsAppTypeSupported(aura::Window* window);

  // A special value to use as an icon identifier for an incognito window.
  static constexpr char kIncognitoWindowIdentifier[] = "incognito_window";

  const base::Uuid& uuid() const { return uuid_; }
  void set_uuid(base::Uuid uuid) { uuid_ = std::move(uuid); }
  DeskTemplateSource source() const { return source_; }
  base::Time created_time() const { return created_time_; }

  void set_updated_time(base::Time updated_time) {
    updated_time_ = updated_time;
  }
  void clear_updated_time() { updated_time_ = base::Time(); }

  const std::u16string& template_name() const { return template_name_; }
  void set_template_name(const std::u16string& template_name) {
    template_name_ = template_name;
  }

  DeskTemplateType type() const { return type_; }

  const ::app_restore::RestoreData* desk_restore_data() const {
    return desk_restore_data_.get();
  }
  ::app_restore::RestoreData* mutable_desk_restore_data() {
    return desk_restore_data_.get();
  }

  void set_desk_restore_data(
      std::unique_ptr<::app_restore::RestoreData> restore_data) {
    desk_restore_data_ = std::move(restore_data);
  }

  void set_client_cache_guid(std::string client_cache_guid) {
    client_cache_guid_ = client_cache_guid;
  }
  const std::string& client_cache_guid() const { return client_cache_guid_; }

  const syncer::DeviceInfo::FormFactor& device_form_factor() const {
    return device_form_factor_;
  }

  void set_device_form_factor(
      const syncer::DeviceInfo::FormFactor& device_form_factor) {
    device_form_factor_ = device_form_factor;
  }

  // The lacros profile ID associated with the saved desk. Only used when type
  // is `kSaveAndRecall`.
  void set_lacros_profile_id(uint64_t lacros_profile_id) {
    lacros_profile_id_ = lacros_profile_id;
  }
  uint64_t lacros_profile_id() const { return lacros_profile_id_; }

  // Used in cases where copies of a DeskTemplate are needed to be made.
  // This specifically used in the DeskSyncBridge which requires a map
  // of DeskTemplate unique pointers to be valid and needs to pass
  // that information in DeskModel callbacks.
  std::unique_ptr<DeskTemplate> Clone() const;

  // Indicates the last time the user created or updated this template.
  // If this desk template was never updated since creation, its creation time
  // will be returned.
  base::Time GetLastUpdatedTime() const {
    return updated_time_.is_null() ? created_time_ : updated_time_;
  }

  // Indicates whether this template has been updated since creation.
  bool WasUpdatedSinceCreation() const { return !updated_time_.is_null(); }

  // Indicates whether this template can be modified by user.
  bool IsModifiable() const { return source_ == DeskTemplateSource::kUser; }

  // This template should launch on startup.
  bool should_launch_on_startup() const { return should_launch_on_startup_; }

  // Sets `desk_uuid` as the desk to launch on for all windows in the template.
  void SetDeskUuid(base::Uuid desk_uuid);

  // Retrieves the base::Value policy definition for this template if it exists.
  // This is used by desks storage to verify that new policies should overwrite
  // stored ones.  Empty values imply user created template, this method will
  // return a base::value::Dict if policy is defined.
  const base::Value& policy_definition() const { return policy_definition_; }

  // Returns `this` in string format. Used for feedback logs.
  std::string ToString() const;

  // Returns `this` in string format. Used for debugging.
  std::string ToDebugString() const;

 private:
  // Returns a string containing basic information for `this`. It could be
  // used for `ToString` and `ToDebugString` according to the given
  // `for_debugging`.
  std::string GetDeskTemplateInfo(bool for_debugging) const;

  base::Uuid uuid_;  // We utilize the string based base::Uuid to uniquely
                     // identify the template.

  // Indicates the source where this desk template originates from.
  const DeskTemplateSource source_;

  // The type of this desk template.
  const DeskTemplateType type_;

  const base::Time created_time_;  // Template creation time.

  // Indicates the last time the user updated this template.
  // If this desk template was never updated since creation, this field will
  // have null value.
  base::Time updated_time_;

  std::u16string template_name_;

  // If this is an admin template, determines if it should be launched on
  // startup.
  bool should_launch_on_startup_ = false;

  // The device sync id associated with this desk template. This is only set
  // for templates saved via `DeskSyncBridge`.
  std::string client_cache_guid_;

  // Form Factor of device this template is from.
  syncer::DeviceInfo::FormFactor device_form_factor_;

  // The lacros profile ID associated with the desk.
  uint64_t lacros_profile_id_ = 0;

  // Contains the app launching and window information that can be used to
  // create a new desk instance with the same set of apps/windows specified in
  // it.
  std::unique_ptr<::app_restore::RestoreData> desk_restore_data_;

  // If this template was originally defined by a policy, store the policy in
  // this field. See GetPolicy for more information.
  base::Value policy_definition_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DESK_TEMPLATE_H_
