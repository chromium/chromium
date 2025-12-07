// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;

import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Java representation of a C++ AwOriginMatchedHeader. */
@JNINamespace("android_webview")
@Lifetime.Temporary
@NullMarked
public class AwOriginMatchedHeader {

    private final String mName;
    private final String mValue;
    private final Set<String> mRules;

    @CalledByNative
    public static AwOriginMatchedHeader create(
            @JniType("std::string") String name,
            @JniType("std::string") String value,
            @JniType("std::vector<std::string>") List<String> rules) {
        return new AwOriginMatchedHeader(name, value, rules);
    }

    /** Normalizes matching rules by removing the default port for http and https patterns. */
    private static Set<String> normalizeRules(Collection<String> rulesFromNative) {
        Set<String> normalizedRules = new HashSet<>(rulesFromNative.size());
        for (String rule : rulesFromNative) {
            normalizedRules.add(removeDefaultPort(rule));
        }
        return normalizedRules;
    }

    private static String removeDefaultPort(String pattern) {
        if (pattern.startsWith("https://") && pattern.endsWith(":443")) {
            return pattern.substring(0, pattern.length() - 4);
        }
        if (pattern.startsWith("http://") && pattern.endsWith(":80")) {
            return pattern.substring(0, pattern.length() - 3);
        }
        return pattern;
    }

    /**
     * Create a new {@link AwOriginMatchedHeader}.
     *
     * <p>The {@code rules} argument is assumed to be a list of valid origin matcher patterns, and
     * will be normalized by this constructor. Normalization means removing the default port values
     * for http and https.
     */
    public AwOriginMatchedHeader(String name, String value, List<String> rules) {
        this.mName = name;
        this.mValue = value;
        this.mRules = Collections.unmodifiableSet(normalizeRules(rules));
    }

    public String getName() {
        return mName;
    }

    public String getValue() {
        return mValue;
    }

    public Set<String> getRules() {
        return mRules;
    }
}
