// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/safety_tips_config.h"

#include "base/no_destructor.h"

namespace {

class SafetyTipsConfigSingleton {
 public:
  void SetProto(
      std::unique_ptr<chrome_browser_safety_tips::SafetyTipsConfig> proto) {
    proto_ = std::move(proto);
  }

  chrome_browser_safety_tips::SafetyTipsConfig* GetProto() const {
    return proto_.get();
  }

  static SafetyTipsConfigSingleton& GetInstance() {
    static base::NoDestructor<SafetyTipsConfigSingleton> instance;
    return *instance;
  }

 private:
  std::unique_ptr<chrome_browser_safety_tips::SafetyTipsConfig> proto_;
};

}  // namespace

// static
void SetSafetyTipsRemoteConfigProto(
    std::unique_ptr<chrome_browser_safety_tips::SafetyTipsConfig> proto) {
  SafetyTipsConfigSingleton::GetInstance().SetProto(std::move(proto));
}

// static
const chrome_browser_safety_tips::SafetyTipsConfig*
GetSafetyTipsRemoteConfigProto() {
  return SafetyTipsConfigSingleton::GetInstance().GetProto();
}
