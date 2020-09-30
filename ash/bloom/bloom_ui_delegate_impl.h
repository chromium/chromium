// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BLOOM_BLOOM_UI_DELEGATE_IMPL_H_
#define ASH_BLOOM_BLOOM_UI_DELEGATE_IMPL_H_

#include "chromeos/components/bloom/public/cpp/bloom_ui_delegate.h"

namespace ash {

class AssistantInteractionController;

class BloomUiDelegateImpl : public chromeos::bloom::BloomUiDelegate {
 public:
  BloomUiDelegateImpl();
  BloomUiDelegateImpl(const BloomUiDelegateImpl&) = delete;
  BloomUiDelegateImpl& operator=(const BloomUiDelegateImpl&) = delete;
  ~BloomUiDelegateImpl() override;

  // BloomUiDelegate implementation:
  void OnInteractionStarted() override;
  void OnShowUI() override;
  void OnShowResult(const std::string& html) override;
  void OnInteractionFinished(
      chromeos::bloom::BloomInteractionResolution resolution) override;

 private:
  AssistantInteractionController* assistant_interaction_controller();
};

}  // namespace ash

#endif  // ASH_BLOOM_BLOOM_UI_DELEGATE_IMPL_H_
