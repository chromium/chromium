// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/intent_helper/factory_reset_delegate.h"

#include "chrome/browser/ash/arc/session/arc_session_manager.h"

namespace arc {

FactoryResetDelegate::FactoryResetDelegate() = default;
FactoryResetDelegate::~FactoryResetDelegate() = default;

void FactoryResetDelegate::ResetArc() {
  ArcSessionManager::Get()->RequestArcDataRemoval();
  ArcSessionManager::Get()->StopAndEnableArc();
}

}  // namespace arc
