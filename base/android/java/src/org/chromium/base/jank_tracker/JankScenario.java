// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

public class JankScenario {
    /**
     * A list of jank scenarios to be tracked, each scenario corresponds to a specific user journey
     * except by PERIODIC_REPORTING, which runs constantly and is uploaded every 30s.
     *
     * <p>IMPORTANT: This must be kept up to date with the histograms.xml histograms
     * (Android.FrameTimelineJank..*) and the JankScenario C++ enum in
     * //base/android/jank_metric_uma_recorder.cc.
     */
    @IntDef({
        Type.PERIODIC_REPORTING,
        Type.OMNIBOX_FOCUS,
        Type.NEW_TAB_PAGE,
        Type.STARTUP,
        Type.TAB_SWITCHER,
        Type.OPEN_LINK_IN_NEW_TAB,
        Type.START_SURFACE_HOMEPAGE,
        Type.START_SURFACE_TAB_SWITCHER,
        Type.FEED_SCROLLING,
        Type.WEBVIEW_SCROLLING,
        Type.COMBINED_WEBVIEW_SCROLLING
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        int PERIODIC_REPORTING = 1;
        int OMNIBOX_FOCUS = 2;
        int NEW_TAB_PAGE = 3;
        int STARTUP = 4;
        int TAB_SWITCHER = 5;
        int OPEN_LINK_IN_NEW_TAB = 6;
        int START_SURFACE_HOMEPAGE = 7;
        int START_SURFACE_TAB_SWITCHER = 8;
        int FEED_SCROLLING = 9;
        int WEBVIEW_SCROLLING = 10;
        int COMBINED_WEBVIEW_SCROLLING = 11;
    }

    public static final JankScenario PERIODIC_REPORTING =
            new JankScenario(Type.PERIODIC_REPORTING, -1);
    public static final JankScenario OMNIBOX_FOCUS = new JankScenario(Type.OMNIBOX_FOCUS, -1);
    public static final JankScenario NEW_TAB_PAGE = new JankScenario(Type.NEW_TAB_PAGE, -1);
    public static final JankScenario STARTUP = new JankScenario(Type.STARTUP, -1);
    public static final JankScenario TAB_SWITCHER = new JankScenario(Type.TAB_SWITCHER, -1);
    public static final JankScenario OPEN_LINK_IN_NEW_TAB =
            new JankScenario(Type.OPEN_LINK_IN_NEW_TAB, -1);
    public static final JankScenario START_SURFACE_HOMEPAGE =
            new JankScenario(Type.START_SURFACE_HOMEPAGE, -1);
    public static final JankScenario START_SURFACE_TAB_SWITCHER =
            new JankScenario(Type.START_SURFACE_TAB_SWITCHER, -1);
    public static final JankScenario FEED_SCROLLING = new JankScenario(Type.FEED_SCROLLING, -1);
    public static final JankScenario WEBVIEW_SCROLLING =
            new JankScenario(Type.WEBVIEW_SCROLLING, -1);
    public static final JankScenario COMBINED_WEBVIEW_SCROLLING =
            new JankScenario(Type.COMBINED_WEBVIEW_SCROLLING, -1);

    private final @Type int mType;
    private final long mId;

    public JankScenario(@Type int type, long id) {
        mType = type;
        mId = id;
    }

    public @Type int type() {
        return mType;
    }

    public long id() {
        return mId;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == null || !(obj instanceof JankScenario)) return false;
        JankScenario other = (JankScenario) obj;
        return mType == other.type() && mId == other.id();
    }

    @Override
    public int hashCode() {
        return Objects.hash(mType, mId);
    }
}
