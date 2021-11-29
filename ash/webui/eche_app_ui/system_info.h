// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_SYSTEM_INFO_H_
#define ASH_WEBUI_ECHE_APP_UI_SYSTEM_INFO_H_

#include <memory>
#include <string>

namespace ash {
namespace eche_app {

// Stores system information for Eche app.
class SystemInfo {
 public:
  class Builder {
   public:
    Builder();
    virtual ~Builder();

    std::unique_ptr<SystemInfo> Build();
    Builder& SetBoardName(const std::string& board_name);
    Builder& SetDeviceName(const std::string& device_name);

   private:
    std::string board_name_;
    std::string device_name_;
  };

  SystemInfo(const SystemInfo& other);
  virtual ~SystemInfo();

  std::string GetDeviceName() const { return device_name_; }
  std::string GetBoardName() const { return board_name_; }

 protected:
  SystemInfo(const std::string& device_name, const std::string& board_name);

 private:
  std::string device_name_;
  std::string board_name_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_SYSTEM_INFO_H_
