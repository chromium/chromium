// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_QUICK_LAUNCH_QUICK_LAUNCH_APPLICATION_H_
#define ASH_COMPONENTS_QUICK_LAUNCH_QUICK_LAUNCH_APPLICATION_H_

#include <memory>

#include "base/macros.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace views {
class AuraInit;
class Widget;
}  // namespace views

namespace quick_launch {

class QuickLaunchApplication : public service_manager::Service {
 public:
  QuickLaunchApplication();
  ~QuickLaunchApplication() override;

  void Quit();

  void set_running_standalone(bool value) { running_standalone_ = value; }

 private:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  views::Widget* window_ = nullptr;

  service_manager::BinderRegistry registry_;

  std::unique_ptr<views::AuraInit> aura_init_;

  bool running_standalone_ = false;

  DISALLOW_COPY_AND_ASSIGN(QuickLaunchApplication);
};

}  // namespace quick_launch

#endif  // ASH_COMPONENTS_QUICK_LAUNCH_QUICK_LAUNCH_APPLICATION_H_
