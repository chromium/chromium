// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.test.util.ViewPrinter;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

/**
 * Represents a fragment, which needs to exist to consider the Station or Facility active.
 *
 * @param <FragmentT> the exact type of fragment expected.
 * @param <ActivityT> the {@link ActivityElement} corresponding to the owning activity.
 */
@NullMarked
public class FragmentElement<FragmentT extends Fragment, ActivityT extends FragmentActivity>
        extends Element<FragmentT> {
    private final ActivityElement<ActivityT> mActivityElement;
    private final Class<FragmentT> mFragmentClass;

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
    public @Nullable ConditionWithResult<FragmentT> createEnterCondition() {
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
            dependOnSupplier(mActivityElement, "Activity");
        }

        @Override
        public String buildDescription() {
            return "Fragment exists and visible: " + mFragmentClass.getSimpleName();
        }

        @Override
        protected ConditionStatusWithResult<FragmentT> resolveWithSuppliers() throws Exception {
            ActivityT activity = mActivityElement.value();
            List<FragmentT> matches = new ArrayList<>();

            // Start the recursive search from the Activity's FragmentManager
            findFragmentsRecursive(activity.getSupportFragmentManager(), matches);

            if (matches.isEmpty()) {
                return awaiting("No fragment found").withoutResult();
            }

            // Check if we found more than one match across the entire tree
            if (matches.size() > 1) {
                List<String> allFragmentInfos = new ArrayList<>();
                for (FragmentT fragment : matches) {
                    View view = fragment.getView();
                    String info =
                            String.format(
                                    "Match Found: %s in Parent [%s]. View: %s, ID: %d, Resumed: %b,"
                                            + " Displ: %s",
                                    fragment.getClass().getSimpleName(),
                                    fragment.getParentFragment(),
                                    view == null
                                            ? null
                                            : ViewPrinter.describeView(
                                                    view, ViewPrinter.Options.PRINT_SHALLOW),
                                    fragment.getId(),
                                    fragment.isResumed(),
                                    view == null ? null : DisplayedPortion.ofView(view));

                    allFragmentInfos.add(info);
                }

                return notFulfilled(
                                "Matched %d Fragments: %s",
                                matches.size(), allFragmentInfos.toString())
                        .withoutResult();
            }

            FragmentT candidate = matches.get(0);

            return fulfilled("Matched fragment: %s", candidate).withResult(candidate);
        }

        /** Helper method to recursively find fragments of a specific class. */
        private void findFragmentsRecursive(FragmentManager fm, List<FragmentT> matches) {
            List<Fragment> fragments = fm.getFragments();
            if (fragments == null) return;

            for (Fragment fragment : fragments) {
                // Filter out fragments that are not displayed and all their children.
                if (fragment == null || !fragment.isResumed() || !fragment.isVisible()) {
                    continue;
                }

                // Dive into children to see if there are more matches.
                findFragmentsRecursive(fragment.getChildFragmentManager(), matches);

                // Filter out fragments with views that are not displayed.
                View view = fragment.getView();
                if (view == null || !view.isAttachedToWindow() || !view.isShown()) {
                    continue;
                }
                DisplayedPortion displayedPortion = DisplayedPortion.ofView(view);
                if (displayedPortion.mPercentage < 1) {
                    continue;
                }

                // Check if this fragment matches the class
                if (!mFragmentClass.isInstance(fragment)) {
                    continue;
                }

                FragmentT matched = mFragmentClass.cast(fragment);
                matches.add(matched);
            }
        }
    }

    private class FragmentNotVisibleCondition extends Condition {
        public FragmentNotVisibleCondition() {
            super(/* isRunOnUiThread= */ false);
        }

        @Override
        protected ConditionStatus checkWithSuppliers() throws Exception {
            ActivityT activity = mActivityElement.value();
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
