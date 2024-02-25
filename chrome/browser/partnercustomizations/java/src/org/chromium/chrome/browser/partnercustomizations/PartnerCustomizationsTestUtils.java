// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import androidx.annotation.Nullable;

import org.chromium.components.embedder_support.util.UrlConstants;

/** Junit test utils for partner browser customizations. */
final class PartnerCustomizationsTestUtils {
    /** Test implementation of {@link HomepageCharacterizationHelper}. */
    static class HomepageCharacterizationHelperStub implements HomepageCharacterizationHelper {
        public static HomepageCharacterizationHelper ntpHelper() {
            return new HomepageCharacterizationHelperStub().setIsPartner(false).setIsNtp(true);
        }

        public static HomepageCharacterizationHelper nonNtpHelper() {
            return new HomepageCharacterizationHelperStub().setIsPartner(true).setIsNtp(false);
        }

        public static HomepageCharacterizationHelper nonPartnerHelper() {
            return new HomepageCharacterizationHelperStub().setIsPartner(false).setIsNtp(false);
        }

        private boolean mIsPartner;
        private boolean mIsNtp;

        private HomepageCharacterizationHelperStub() {}

        @Override
        public boolean isUrlNtp(@Nullable String url) {
            return UrlConstants.NTP_URL.equals(url);
        }

        @Override
        public boolean isPartner() {
            return mIsPartner;
        }

        @Override
        public boolean isNtp() {
            return mIsNtp;
        }

        private HomepageCharacterizationHelperStub setIsPartner(boolean isPartner) {
            mIsPartner = isPartner;
            return this;
        }

        private HomepageCharacterizationHelperStub setIsNtp(boolean isNtp) {
            mIsNtp = isNtp;
            return this;
        }
    }
}
