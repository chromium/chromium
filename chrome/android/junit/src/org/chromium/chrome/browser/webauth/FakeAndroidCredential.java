// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import android.os.Bundle;

/** Interface of the Android Credential Manager Credential object. */
interface FakeAndroidCredential {
    Bundle getData();

    String getType();
}
