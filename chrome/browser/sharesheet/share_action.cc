// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/share_action.h"

namespace sharesheet {

bool ShareAction::ShouldShowAction(const apps::mojom::IntentPtr& intent,
                                   bool contains_hosted_document) {
  return !contains_hosted_document;
}

}  // namespace sharesheet
