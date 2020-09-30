// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * We use this annotation, together with {@FeedDataInjectRule}, to tell
 * which data file to inject for each test cases.
 *
 * For instance, if file_foo is used in test A, file_bar is used
 * in test B.
 *
 *     @Rule
 *     public FeedDataInjectRule mDataInjector = new FeedDataInjectRule();
 *
 *     @DataFilePath("file_foo")
 *     public void test_A() {
 *        // Write the test case here.
 *     }
 *
 *     @DataFilePath("file_bar")
 *     public void test_B() {
 *        // Write the test case here.
 *     }
 *
 *  In test_A, the FeedDataInjectRule will then injects file_foo via testNetworkClient,
 *  invokes triggerRefresh at UI thread, responses the seeded mockserver response from
 *  file_foo, and then blocks waiting for the content change notification. It does the
 *  similar for test_B except it injects file_bar.
 */
@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface DataFilePath {
    /**
     * @return one DataFilePath.
     */
    public String value();
}
