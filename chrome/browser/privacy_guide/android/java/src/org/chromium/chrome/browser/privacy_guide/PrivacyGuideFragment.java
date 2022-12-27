// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.viewpager2.widget.ViewPager2;

import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayoutMediator;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.widget.ButtonCompat;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Fragment containing the Privacy Guide (a walk-through of the most important privacy settings).
 */
public class PrivacyGuideFragment extends Fragment {
    /**
     * The types of fragments supported. Each fragment corresponds to a step in the privacy guide.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({FragmentType.MSBB, FragmentType.HISTORY_SYNC, FragmentType.SAFE_BROWSING,
            FragmentType.COOKIES})
    @interface FragmentType {
        int MSBB = 0;
        int HISTORY_SYNC = 1;
        int SAFE_BROWSING = 2;
        int COOKIES = 3;
        int MAX_VALUE = COOKIES;
    }

    private BottomSheetController mBottomSheetController;
    private PrivacyGuidePagerAdapter mPagerAdapter;
    private View mView;
    private ViewPager2 mViewPager;
    private ButtonCompat mNextButton;
    private ButtonCompat mBackButton;
    private ButtonCompat mFinishButton;
    private PrivacyGuideMetricsDelegate mPrivacyGuideMetricsDelegate;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(true);
        mPrivacyGuideMetricsDelegate = new PrivacyGuideMetricsDelegate();
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        modifyAppBar();

        mView = inflater.inflate(R.layout.privacy_guide_fragment, container, false);
        displayWelcomePage();

        return mView;
    }

    private void modifyAppBar() {
        AppCompatActivity settingsActivity = (AppCompatActivity) getActivity();
        settingsActivity.setTitle(R.string.prefs_privacy_guide_title);
        settingsActivity.getSupportActionBar().setDisplayHomeAsUpEnabled(false);
    }

    private void displayWelcomePage() {
        FrameLayout content = mView.findViewById(R.id.fragment_content);
        content.removeAllViews();
        getLayoutInflater().inflate(R.layout.privacy_guide_welcome, content);

        ButtonCompat welcomeButton = (ButtonCompat) mView.findViewById(R.id.start_button);
        welcomeButton.setOnClickListener((View v) -> displayMainFlow());
    }

    private void displayMainFlow() {
        // Record that the user clicked the next button on the welcome card
        PrivacyGuideMetricsDelegate.recordMetricsForWelcomeCard();
        FrameLayout content = mView.findViewById(R.id.fragment_content);
        content.removeAllViews();
        getLayoutInflater().inflate(R.layout.privacy_guide_steps, content);

        mViewPager = (ViewPager2) mView.findViewById(R.id.review_viewpager);
        mPagerAdapter = new PrivacyGuidePagerAdapter(this, new StepDisplayHandlerImpl());
        mViewPager.setAdapter(mPagerAdapter);
        mViewPager.setUserInputEnabled(false);

        // Record the initial state of the first card
        mPrivacyGuideMetricsDelegate.setInitialStateForCard(
                mPagerAdapter.getFragmentType(mViewPager.getCurrentItem()));

        TabLayout tabLayout = mView.findViewById(R.id.tab_layout);
        new TabLayoutMediator(tabLayout, mViewPager, (tab, position) -> {
            tab.view.setClickable(false);
        }).attach();

        mNextButton = (ButtonCompat) mView.findViewById(R.id.next_button);
        mNextButton.setOnClickListener((View v) -> nextStep());

        mBackButton = (ButtonCompat) mView.findViewById(R.id.back_button);
        mBackButton.setOnClickListener((View v) -> previousStep());

        mFinishButton = (ButtonCompat) mView.findViewById(R.id.finish_button);
        mFinishButton.setOnClickListener((View v) -> displayDonePage());
    }

    private void displayDonePage() {
        // Record metrics when the user clicks the next button on the final card
        mPrivacyGuideMetricsDelegate.recordMetricsOnNextForCard(
                mPagerAdapter.getFragmentType(mViewPager.getCurrentItem()));

        FrameLayout content = mView.findViewById(R.id.fragment_content);
        content.removeAllViews();
        getLayoutInflater().inflate(R.layout.privacy_guide_done, content);

        ButtonCompat doneButton = (ButtonCompat) mView.findViewById(R.id.done_button);
        doneButton.setOnClickListener((View v) -> {
            PrivacyGuideMetricsDelegate.recordMetricsForDoneButton();
            getActivity().onBackPressed();
        });
    }

    private void nextStep() {
        int currentIdx = mViewPager.getCurrentItem();
        int nextIdx = currentIdx + 1;
        if (nextIdx < mPagerAdapter.getItemCount()) {
            mViewPager.setCurrentItem(nextIdx);
        }
        mBackButton.setVisibility(View.VISIBLE);
        if (nextIdx + 1 == mPagerAdapter.getItemCount()) {
            mNextButton.setVisibility(View.GONE);
            mFinishButton.setVisibility(View.VISIBLE);
        }

        // Record metrics when the user clicks the next button
        mPrivacyGuideMetricsDelegate.recordMetricsOnNextForCard(
                mPagerAdapter.getFragmentType(currentIdx));
        // Record the initial state of the next card
        mPrivacyGuideMetricsDelegate.setInitialStateForCard(mPagerAdapter.getFragmentType(nextIdx));
    }

    private void previousStep() {
        mFinishButton.setVisibility(View.GONE);
        int currentIdx = mViewPager.getCurrentItem();
        int prevIdx = currentIdx - 1;
        if (prevIdx >= 0) {
            mViewPager.setCurrentItem(prevIdx);
        }
        mNextButton.setVisibility(View.VISIBLE);
        if (prevIdx == 0) {
            mBackButton.setVisibility(View.INVISIBLE);
        }

        // Record metrics when the user clicks the back button
        PrivacyGuideMetricsDelegate.recordMetricsOnBackForCard(
                mPagerAdapter.getFragmentType(currentIdx));
        // Record the initial state of the previous card
        mPrivacyGuideMetricsDelegate.setInitialStateForCard(mPagerAdapter.getFragmentType(prevIdx));
    }

    @Override
    public void onAttachFragment(@NonNull Fragment childFragment) {
        if (childFragment instanceof SafeBrowsingFragment) {
            ((SafeBrowsingFragment) childFragment).setBottomSheetController(mBottomSheetController);
        }
    }

    @Override
    public void onCreateOptionsMenu(@NonNull Menu menu, @NonNull MenuInflater inflater) {
        super.onCreateOptionsMenu(menu, inflater);
        menu.clear();
        inflater.inflate(R.menu.privacy_guide_toolbar_menu, menu);
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        if (item.getItemId() == R.id.close_menu_id) {
            getActivity().onBackPressed();
            return true;
        }

        return false;
    }

    public void setBottomSheetController(BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    void setPrivacyGuideMetricsDelegateForTesting(
            @Nullable PrivacyGuideMetricsDelegate privacyGuideMetricsDelegate) {
        mPrivacyGuideMetricsDelegate = privacyGuideMetricsDelegate;
    }
}
