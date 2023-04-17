// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DESK_TEMPLATE_H_
#define ASH_PUBLIC_CPP_DESK_TEMPLATE_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/app_restore/restore_data.h"

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

  DeskTemplate(const DeskTemplate&) = delete;
  DeskTemplate& operator=(const DeskTemplate&) = delete;
  ~DeskTemplate();

  // Returns whether desk templates support the `window`'s app type.
  static bool IsAppTypeSupported(aura::Window* window);

  // A special value to use as an icon identifier for an incognito window.
  static constexpr char kIncognitoWindowIdentifier[] = "incognito_window";

  const base::Uuid& uuid() const { return uuid_; }
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

  void set_launch_id(int32_t launch_id) { launch_id_ = launch_id; }
  int32_t launch_id() const { return launch_id_; }

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

  // Sets `desk_index` as the desk to launch on for all windows in the template.
  void SetDeskIndex(int desk_index);

  // Returns `this` in string format. Used for feedback logs.
  std::string ToString() const;

  // Returns `this` in string format. Used for debugging.
  std::string ToDebugString() const;

 private:
  // Returns a string containing basic information for `this`. It could be used
  // for `ToString` and `ToDebugString` according to the given `for_debugging`.
  std::string GetDeskTemplateInfo(bool for_debugging) const;

  const base::Uuid uuid_;  // We utilize the string based base::Uuid to uniquely
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

  // The id associated with a particular launch of this template. Must be
  // positive when launching.
  int32_t launch_id_ = 0;

  // Contains the app launching and window information that can be used to
  // create a new desk instance with the same set of apps/windows specified in
  // it.
  std::unique_ptr<::app_restore::RestoreData> desk_restore_data_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DESK_TEMPLATE_H_
