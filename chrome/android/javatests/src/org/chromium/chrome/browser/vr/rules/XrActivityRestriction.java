// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import androidx.annotation.IntDef;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * An annotation for listing what types of ChromeActivity the test should be restricted to. This is
 * meant to only be used with test classes that have a XrActivityRestrictionRule, otherwise the
 * annotation will have no effect.
 *
 * <p>For example, the following would restrict a test to only run in ChromeTabbedActivity and
 * CustomTabActivity: <code>
 *     @XrActivityRestriction({XrActivityRestriction.CTA, XrActivityRestriction.CCT})
 *     </code> If a test is not annotated with this and XrActivityRestrictionRule is present, the
 * test will default to only running in ChromeTabbedActivity (regular Chrome).
 */
@Target({ElementType.METHOD})
@Retention(RetentionPolicy.RUNTIME)
public @interface XrActivityRestriction {
    @IntDef({
        SupportedActivity.CTA,
        SupportedActivity.CCT,
        SupportedActivity.WAA,
        SupportedActivity.ALL
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SupportedActivity {
        int CTA = 0; // ChromeTabbedActivity/Normal Chrome
        int CCT = 1; // CustomTabActivity/Chrome Custom Tab
        int WAA = 2; // WebappActivity/Progressive Web App
        int ALL = 3; // Run in all of the above
    }

    /**
     * @return A list of activity restrictions.
     */
    @SupportedActivity
    public int[] value();
}
