// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/proactive_suggestions_client.h"

#include "base/logging.h"

namespace ash {

namespace {

ProactiveSuggestionsClient* g_instance = nullptr;

}  // namespace

// static
ProactiveSuggestionsClient* ProactiveSuggestionsClient::Get() {
  return g_instance;
}

ProactiveSuggestionsClient::ProactiveSuggestionsClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

ProactiveSuggestionsClient::~ProactiveSuggestionsClient() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
