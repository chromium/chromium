// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.chrome.R;

// TODO(crbug.com/870056, crbug.com/884321): This class purely exists to silence lint errors. Remove
// this class once we have moved resources into VR DFM.
/* package */ class SilenceLintErrors {
    private int[] mRes = new int[] {
            R.string.vr_shell_feedback_infobar_feedback_button,
            R.string.vr_shell_feedback_infobar_description,
            R.string.vr_services_check_infobar_install_text,
            R.string.vr_services_check_infobar_update_text,
            R.string.vr_services_check_infobar_install_button,
            R.string.vr_services_check_infobar_update_button, R.anim.stay_hidden,
            R.drawable.vr_services, R.string.vr_module_title, R.string.vr_module_install_start_text,
            R.string.vr_module_install_success_text, R.string.vr_module_install_failure_text,
    };

    private SilenceLintErrors() {}
}
