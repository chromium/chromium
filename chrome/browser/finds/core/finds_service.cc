// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_service.h"

namespace finds {

FindsService::FindsService() = default;

FindsService::~FindsService() = default;

void FindsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FindsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace finds
