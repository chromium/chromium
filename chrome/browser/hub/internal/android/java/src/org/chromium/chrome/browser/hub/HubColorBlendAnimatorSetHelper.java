// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.animation.Animator;
import android.animation.AnimatorSet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

/** Helper class to manage Animator object creation for views during a hub color scheme change. */
@NullMarked
public class HubColorBlendAnimatorSetHelper {
    private List<HubViewColorBlend> mColorBlendList;
    private @Nullable @HubColorScheme Integer mPrevColorScheme;
    private @Nullable @HubColorScheme Integer mNewColorScheme;

    /** Default Constructor. */
    public HubColorBlendAnimatorSetHelper() {
        mColorBlendList = new ArrayList<>();
    }

    /**
     * Set the list of {@link HubViewColorBlend} objects which will be used to create the Animators
     * in the AnimatorSet. The primary use of this method is for dependency injection during
     * testing.
     */
    public HubColorBlendAnimatorSetHelper setColorBlendList(
            List<HubViewColorBlend> colorBlendList) {
        mColorBlendList = colorBlendList;
        return this;
    }

    /**
     * Add a {@link HubViewColorBlend} which will construct an animator that will be added to the
     * end AnimatorSet.
     */
    public HubColorBlendAnimatorSetHelper registerBlend(HubViewColorBlend colorBlend) {
        mColorBlendList.add(colorBlend);
        return this;
    }

    /** Set the previous color scheme of the Hub which will be used in animations. */
    public HubColorBlendAnimatorSetHelper setNewColorScheme(@HubColorScheme int colorScheme) {
        mNewColorScheme = colorScheme;
        return this;
    }

    /** Set the new color scheme of the Hub which will be used in animations. */
    public HubColorBlendAnimatorSetHelper setPreviousColorScheme(@HubColorScheme int colorScheme) {
        mPrevColorScheme = colorScheme;
        return this;
    }

    /** Checks if any animations have been added to the builder. */
    public boolean hasAnimations() {
        return !mColorBlendList.isEmpty();
    }

    /** Constructs the AnimatorSet. */
    public AnimatorSet build() {
        assert !mColorBlendList.isEmpty();
        assert mPrevColorScheme != null;
        assert mNewColorScheme != null;

        List<Animator> animatorsList = new ArrayList<>();
        for (HubViewColorBlend colorBlend : mColorBlendList) {
            animatorsList.add(
                    colorBlend.createAnimationForTransition(mPrevColorScheme, mNewColorScheme));
        }

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(animatorsList);
        return animatorSet;
    }
}
