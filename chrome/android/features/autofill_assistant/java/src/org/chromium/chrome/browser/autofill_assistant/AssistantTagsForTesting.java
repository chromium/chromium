// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

/**
 * Contains tags used for java UI tests.
 *
 * These tags are used to tag dynamically created views (i.e., without resource ID) to allow tests
 * to easily retrieve and test them.
 */
public class AssistantTagsForTesting {
    public static final String COLLECT_USER_DATA_ACCORDION_TAG = "accordion";
    public static final String COLLECT_USER_DATA_LOGIN_SECTION_TAG = "login";
    public static final String COLLECT_USER_DATA_CONTACT_DETAILS_SECTION_TAG = "contact";
    public static final String COLLECT_USER_DATA_PAYMENT_METHOD_SECTION_TAG = "payment";
    public static final String COLLECT_USER_DATA_SHIPPING_ADDRESS_SECTION_TAG = "shipping";
    public static final String COLLECT_USER_DATA_DATE_RANGE_START_TAG = "date_start";
    public static final String COLLECT_USER_DATA_DATE_RANGE_END_TAG = "date_end";
    public static final String COLLECT_USER_DATA_TERMS_SECTION_TAG = "terms";
    public static final String COLLECT_USER_DATA_TERMS_REQUIRE_REVIEW = "require_review";
    public static final String VERTICAL_EXPANDER_CHEVRON = "chevron";
    public static final String COLLECT_USER_DATA_CHOICE_LIST = "choicelist";
}
