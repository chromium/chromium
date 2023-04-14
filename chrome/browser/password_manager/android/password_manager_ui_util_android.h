// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_UI_UTIL_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_UI_UTIL_ANDROID_H_

#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "content/public/browser/web_contents.h"

// True if the focus event was sent for the current focused frame or if it is
// a blur event and no frame is focused. This check avoids reacting to
// obsolete events that arrived in an unexpected order.
bool ShouldAcceptFocusEvent(
    content::WebContents* web_contents,
    password_manager::ContentPasswordManagerDriver* driver,
    autofill::mojom::FocusedFieldType focused_field_type);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_UI_UTIL_ANDROID_H_
