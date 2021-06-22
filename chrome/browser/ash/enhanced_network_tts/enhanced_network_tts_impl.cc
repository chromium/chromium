// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/enhanced_network_tts/enhanced_network_tts_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/no_destructor.h"

namespace ash {

namespace {

// TODO(crbug.com/1217301): Replace this placeholder implementation with the
// real data fetching logic.
std::vector<uint8_t> FetchData(const std::string& text) {
  int n = text.length();
  std::vector<uint8_t> data;
  for (int i = 0; i < n; i++) {
    data.push_back(uint8_t{text[i]});
  }
  return data;
}

}  // namespace

EnhancedNetworkTtsImpl& EnhancedNetworkTtsImpl::GetInstance() {
  static base::NoDestructor<EnhancedNetworkTtsImpl> tts_impl;
  return *tts_impl;
}

EnhancedNetworkTtsImpl::EnhancedNetworkTtsImpl() = default;
EnhancedNetworkTtsImpl::~EnhancedNetworkTtsImpl() = default;

void EnhancedNetworkTtsImpl::BindReceiver(
    mojo::PendingReceiver<enhanced_network_tts::mojom::EnhancedNetworkTts>
        receiver) {
  // Reset the receiver in case of rebinding (e.g., after the extension crash).
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void EnhancedNetworkTtsImpl::GetAudioData(const std::string& text,
                                          GetAudioDataCallback callback) {
  std::move(callback).Run(FetchData(text));
}

}  // namespace ash
