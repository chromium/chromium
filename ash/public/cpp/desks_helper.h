// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DESKS_HELPER_H_
#define ASH_PUBLIC_CPP_DESKS_HELPER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/full_restore/restore_data.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Class to represent a desk template. It can be used to create a desk with
// a certain set of application windows specified in |desk_restore_data_|.
class ASH_PUBLIC_EXPORT DeskTemplate {
 public:
  DeskTemplate();
  explicit DeskTemplate(double uuid);
  DeskTemplate(const DeskTemplate&) = delete;
  DeskTemplate& operator=(const DeskTemplate&) = delete;
  ~DeskTemplate();

  double uuid() const { return uuid_; }
  std::u16string desk_name() const { return desk_name_; }
  void set_desk_name(const std::u16string& desk_name) {
    desk_name_ = desk_name;
  }
  full_restore::RestoreData* desk_restore_data() {
    return desk_restore_data_.get();
  }
  void set_desk_restore_data(
      std::unique_ptr<full_restore::RestoreData> restore_data) {
    desk_restore_data_ = std::move(restore_data);
  }

 private:
  const double uuid_;  // we'll use the current time in seconds to uniquely
                       // identify the template.
  std::u16string desk_name_;

  // Contains the app launching and window information that can be used to
  // re-launch/restore this desk.
  std::unique_ptr<full_restore::RestoreData> desk_restore_data_;
};

// Interface for an ash client (e.g. Chrome) to interact with the Virtual Desks
// feature.
class ASH_PUBLIC_EXPORT DesksHelper {
 public:
  static DesksHelper* Get();

  // Returns true if |window| exists on the currently active desk.
  virtual bool BelongsToActiveDesk(aura::Window* window) = 0;

  // Returns the active desk's index.
  virtual int GetActiveDeskIndex() const = 0;

  // Returns the names of the desk at |index|. If |index| is out-of-bounds,
  // return empty string.
  virtual std::u16string GetDeskName(int index) const = 0;

  // Returns the number of desks.
  virtual int GetNumberOfDesks() const = 0;

  // Sends |window| to desk at |desk_index|. Does nothing if the desk at
  // |desk_index| is the active desk. |desk_index| must be valid.
  virtual void SendToDeskAtIndex(aura::Window* window, int desk_index) = 0;

  // Captures the active desk and returns it as a desk template containing
  // necessary information that can be used to create a same desk.
  virtual std::unique_ptr<DeskTemplate> CaptureActiveDeskAsTemplate(
      const base::FilePath& profile_path) = 0;

  // Creates and activates a new desk for a template with name `desk_name`. Runs
  // `callback` with true if creation was successful, false otherwise.
  virtual void CreateAndActivateNewDeskForTemplate(
      const std::u16string& desk_name,
      base::OnceCallback<void(bool)> callback) = 0;

 protected:
  DesksHelper();
  virtual ~DesksHelper();

 private:
  DISALLOW_COPY_AND_ASSIGN(DesksHelper);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DESKS_HELPER_H_
