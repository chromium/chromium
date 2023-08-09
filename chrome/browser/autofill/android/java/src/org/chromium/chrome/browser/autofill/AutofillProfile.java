// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

// TODO(crbug/1079268): Delete this class once downstream switches to //components.
public class AutofillProfile extends org.chromium.components.autofill.AutofillProfile {
    public AutofillProfile(org.chromium.components.autofill.AutofillProfile profile) {
        super(profile);
    }

    // Overridden just the minimum set of methods used downstream.
    public static class Builder extends org.chromium.components.autofill.AutofillProfile.Builder {
        @Override
        public Builder setGUID(String guid) {
            super.setGUID(guid);
            return this;
        }
        @Override
        public Builder setFullName(String fullName) {
            super.setFullName(fullName);
            return this;
        }
        @Override
        public Builder setCompanyName(String companyName) {
            super.setCompanyName(companyName);
            return this;
        }
        @Override
        public Builder setStreetAddress(String streetAddress) {
            super.setStreetAddress(streetAddress);
            return this;
        }
        @Override
        public Builder setRegion(String region) {
            super.setRegion(region);
            return this;
        }
        @Override
        public Builder setLocality(String locality) {
            super.setLocality(locality);
            return this;
        }
        @Override
        public Builder setPostalCode(String postalCode) {
            super.setPostalCode(postalCode);
            return this;
        }
        @Override
        public Builder setCountryCode(String countryCode) {
            super.setCountryCode(countryCode);
            return this;
        }
        @Override
        public Builder setPhoneNumber(String phoneNumber) {
            super.setPhoneNumber(phoneNumber);
            return this;
        }
        @Override
        public Builder setEmailAddress(String emailAddress) {
            super.setEmailAddress(emailAddress);
            return this;
        }
        @Override
        public AutofillProfile build() {
            return new AutofillProfile(super.build());
        }
    }

    public static Builder builder() {
        return new Builder();
    }
}
