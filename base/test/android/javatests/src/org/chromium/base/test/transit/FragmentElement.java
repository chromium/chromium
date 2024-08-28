// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;

import java.util.List;

/**
 * Represents a fragment, which needs to exist to consider the Station or Facility active.
 *
 * @param <FragmentT> the exact type of fragment expected.
 * @param <ActivityT> the {@link ActivityElement} corresponding to the owning activity.
 */
public class FragmentElement<FragmentT extends Fragment, ActivityT extends FragmentActivity>
        extends Element<FragmentT> {
    private ActivityElement<ActivityT> mActivityElement;
    private Class<FragmentT> mFragmentClass;

    /**
     * Constructor.
     *
     * @param fragmentClass the exact type of the fragment expected to appear in the view.
     * @param activityElement the element for the activity owning the fragment.
     */
    public FragmentElement(
            Class<FragmentT> fragmentClass, ActivityElement<ActivityT> activityElement) {
        super("FE/" + fragmentClass.getCanonicalName());
        mActivityElement = activityElement;
        mFragmentClass = fragmentClass;
    }

    @Override
    public ConditionWithResult<FragmentT> createEnterCondition() {
        return new FragmentExistsCondition();
    }

    @Nullable
    @Override
    public Condition createExitCondition() {
        return new FragmentNotVisibleCondition();
    }

    private class FragmentExistsCondition extends ConditionWithResult<FragmentT> {
        public FragmentExistsCondition() {
            super(/* isRunOnUiThread= */ false);
        }

        @Override
        public String buildDescription() {
            return "Fragment exists and visible: " + mFragmentClass.getSimpleName();
        }

        @Override
        protected ConditionStatusWithResult<FragmentT> resolveWithSuppliers() throws Exception {
            ActivityT activity = mActivityElement.get();
            if (activity == null) return awaiting("No activity found").withoutResult();
            List<Fragment> fragments = activity.getSupportFragmentManager().getFragments();
            FragmentT candidate = null;
            for (Fragment fragment : fragments) {
                if (mFragmentClass.equals(fragment.getClass())) {
                    FragmentT matched = mFragmentClass.cast(fragment);
                    if (candidate != null) {
                        return error("%s matched two Fragments: %s, %s", this, candidate, matched)
                                .withoutResult();
                    }
                    candidate = matched;
                }
            }
            if (candidate == null) {
                return awaiting("No fragment found").withoutResult();
            }
            if (!candidate.isVisible()) {
                return awaiting("Fragment is not visible").withoutResult();
            }
            return fulfilled("Matched fragment: %s", candidate).withResult(candidate);
        }
    }

    private class FragmentNotVisibleCondition extends Condition {
        public FragmentNotVisibleCondition() {
            super(/* isRunOnUiThread= */ false);
        }

        @Override
        protected ConditionStatus checkWithSuppliers() throws Exception {
            ActivityT activity = mActivityElement.get();
            if (activity == null) return fulfilled("Fragment's owning activity is destroyed");
            List<Fragment> fragments = activity.getSupportFragmentManager().getFragments();
            for (Fragment fragment : fragments) {
                if (mFragmentClass.equals(fragment.getClass()) && fragment.isVisible()) {
                    return error("Fragment %s is still visible, although not expected.", fragment);
                }
            }
            return fulfilled("Fragment is destroyed");
        }

        @Override
        public String buildDescription() {
            return "Fragment does not exist or not visible: " + mFragmentClass.getSimpleName();
        }
    }
}
