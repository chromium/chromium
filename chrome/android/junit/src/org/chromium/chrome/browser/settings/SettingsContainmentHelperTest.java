// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentManager.FragmentLifecycleCallbacks;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceManager;
import androidx.preference.PreferenceScreen;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.PreferenceUpdateObserver;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemDecoration;

/** Unit tests for {@link SettingsContainmentHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsContainmentHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SettingsContainmentHelper.Delegate mDelegate;
    @Mock private FragmentManager mFragmentManager;
    @Mock private PreferenceUpdateObserver mObserver;
    @Mock private PreferenceFragmentCompat mPreferenceFragment;
    @Mock private View mView;
    @Mock private ViewTreeObserver mViewTreeObserver;
    @Mock private MainSettings mMainSettings;
    @Mock private MultiColumnSettings mMultiColumnSettings;
    @Captor private ArgumentCaptor<FragmentLifecycleCallbacks> mCallbackCaptor;
    @Captor private ArgumentCaptor<ViewTreeObserver.OnGlobalLayoutListener> mListenerCaptor;

    private Context mContext;
    private SettingsContainmentHelper mContainmentHelper;

    private static class TestProviderFragment extends Fragment
            implements PreferenceUpdateObserver.Provider {
        private @Nullable PreferenceUpdateObserver mObserver;

        @Override
        public void setPreferenceUpdateObserver(PreferenceUpdateObserver observer) {
            mObserver = observer;
        }

        @Override
        public void removePreferenceUpdateObserver() {
            mObserver = null;
        }
    }

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_Chromium_Settings);
        when(mDelegate.getPreferenceUpdateObserver()).thenReturn(mObserver);
        mContainmentHelper = new SettingsContainmentHelper(mContext, mDelegate);
    }

    @Test
    public void testRegisterCallbacks() {
        mContainmentHelper.registerCallbacks(mFragmentManager);
        verify(mFragmentManager).registerFragmentLifecycleCallbacks(any(), eq(true));
    }

    @Test
    public void testUnregisterCallbacks() {
        mContainmentHelper.registerCallbacks(mFragmentManager);
        mContainmentHelper.unregisterCallbacks(mFragmentManager);
        verify(mFragmentManager).unregisterFragmentLifecycleCallbacks(any());
    }

    @Test
    public void testOnFragmentAttached_setsObserver() {
        mContainmentHelper.registerCallbacks(mFragmentManager);
        verify(mFragmentManager)
                .registerFragmentLifecycleCallbacks(mCallbackCaptor.capture(), eq(true));
        FragmentManager.FragmentLifecycleCallbacks callbacks = mCallbackCaptor.getValue();

        TestProviderFragment fragment = new TestProviderFragment();
        callbacks.onFragmentAttached(mFragmentManager, fragment, mContext);

        assertEquals(mObserver, fragment.mObserver);
    }

    @Test
    public void testOnFragmentDetached_removesObserver() {
        mContainmentHelper.registerCallbacks(mFragmentManager);
        verify(mFragmentManager)
                .registerFragmentLifecycleCallbacks(mCallbackCaptor.capture(), eq(true));
        FragmentManager.FragmentLifecycleCallbacks callbacks = mCallbackCaptor.getValue();

        TestProviderFragment fragment = new TestProviderFragment();
        fragment.setPreferenceUpdateObserver(mObserver);

        callbacks.onFragmentDetached(mFragmentManager, fragment);

        assertNull(fragment.mObserver);
    }

    @Test
    public void testOnFragmentViewCreated_addsLayoutListener() {
        mContainmentHelper.registerCallbacks(mFragmentManager);
        verify(mFragmentManager)
                .registerFragmentLifecycleCallbacks(mCallbackCaptor.capture(), eq(true));
        FragmentManager.FragmentLifecycleCallbacks callbacks = mCallbackCaptor.getValue();

        when(mPreferenceFragment.getView()).thenReturn(mView);
        when(mView.getViewTreeObserver()).thenReturn(mViewTreeObserver);

        callbacks.onFragmentViewCreated(mFragmentManager, mPreferenceFragment, mView, null);

        verify(mViewTreeObserver).addOnGlobalLayoutListener(any());
    }

    @Test
    public void testPostUpdateContainmentOnLayout_addsLayoutListener() {
        when(mPreferenceFragment.getView()).thenReturn(mView);
        when(mView.getViewTreeObserver()).thenReturn(mViewTreeObserver);

        mContainmentHelper.postUpdateContainmentOnLayout(mPreferenceFragment);

        verify(mViewTreeObserver).addOnGlobalLayoutListener(any());
    }

    @Test
    public void testPostUpdateContainmentOnLayout_removesExistingListenerBeforeAddingNew() {
        when(mPreferenceFragment.getView()).thenReturn(mView);
        when(mView.getViewTreeObserver()).thenReturn(mViewTreeObserver);

        // First call registers an initial layout listener.
        mContainmentHelper.postUpdateContainmentOnLayout(mPreferenceFragment);
        verify(mViewTreeObserver).addOnGlobalLayoutListener(mListenerCaptor.capture());
        ViewTreeObserver.OnGlobalLayoutListener firstListener = mListenerCaptor.getValue();

        // Second call must unregister the previous listener before registering a new one
        // to prevent duplicate triggers and listener leaks on rapid successive updates.
        mContainmentHelper.postUpdateContainmentOnLayout(mPreferenceFragment);
        verify(mViewTreeObserver).removeOnGlobalLayoutListener(firstListener);
        verify(mViewTreeObserver, times(2)).addOnGlobalLayoutListener(any());
    }

    @Test
    public void testPostUpdateContainmentOnLayout_onGlobalLayoutTriggersUpdateAndCleansUp() {
        when(mPreferenceFragment.getView()).thenReturn(mView);
        when(mView.getViewTreeObserver()).thenReturn(mViewTreeObserver);
        when(mPreferenceFragment.getContext()).thenReturn(mContext);

        RecyclerView recyclerView = createRecyclerView();
        RecyclerView.Adapter expectedAdapter = recyclerView.getAdapter();
        setFragmentList(mPreferenceFragment, recyclerView);

        PreferenceManager preferenceManager = new PreferenceManager(mContext);
        PreferenceScreen preferenceScreen = preferenceManager.createPreferenceScreen(mContext);
        when(mPreferenceFragment.getPreferenceScreen()).thenReturn(preferenceScreen);

        mContainmentHelper.postUpdateContainmentOnLayout(mPreferenceFragment);
        verify(mViewTreeObserver).addOnGlobalLayoutListener(mListenerCaptor.capture());
        ViewTreeObserver.OnGlobalLayoutListener listener = mListenerCaptor.getValue();

        // Simulate global layout completion pass.
        listener.onGlobalLayout();

        // Verify listener removes itself to prevent redundant future invocations,
        // attaches ContainmentItemDecoration, and preserves adapter without view re-inflation.
        verify(mViewTreeObserver).removeOnGlobalLayoutListener(listener);
        assertEquals(1, recyclerView.getItemDecorationCount());
        assertEquals(
                ContainmentItemDecoration.class, recyclerView.getItemDecorationAt(0).getClass());
        assertEquals(expectedAdapter, recyclerView.getAdapter());
    }

    @Test
    public void testUpdateFragmentContainment_singleColumn_genericFragment() {
        when(mDelegate.isTwoColumnSettingsVisible()).thenReturn(false);

        when(mPreferenceFragment.getContext()).thenReturn(mContext);

        RecyclerView recyclerView = createRecyclerView();
        RecyclerView.Adapter expectedAdapter = recyclerView.getAdapter();
        setFragmentList(mPreferenceFragment, recyclerView);

        PreferenceManager preferenceManager = new PreferenceManager(mContext);
        PreferenceScreen preferenceScreen = preferenceManager.createPreferenceScreen(mContext);
        when(mPreferenceFragment.getPreferenceScreen()).thenReturn(preferenceScreen);

        mContainmentHelper.updateFragmentContainment(mPreferenceFragment);

        assertEquals(1, recyclerView.getItemDecorationCount());
        assertEquals(
                ContainmentItemDecoration.class, recyclerView.getItemDecorationAt(0).getClass());
        assertEquals(expectedAdapter, recyclerView.getAdapter());
    }

    @Test
    public void testUpdateFragmentContainment_singleColumn_mainSettings() {
        when(mDelegate.isTwoColumnSettingsVisible()).thenReturn(false);

        when(mMainSettings.getContext()).thenReturn(mContext);

        RecyclerView recyclerView = createRecyclerView();
        RecyclerView.Adapter expectedAdapter = recyclerView.getAdapter();
        setFragmentList(mMainSettings, recyclerView);

        PreferenceManager preferenceManager = new PreferenceManager(mContext);
        PreferenceScreen preferenceScreen = preferenceManager.createPreferenceScreen(mContext);
        when(mMainSettings.getPreferenceScreen()).thenReturn(preferenceScreen);

        mContainmentHelper.updateFragmentContainment(mMainSettings);

        // Verify MainSettings specific call
        verify(mMainSettings).setMultiColumnSettings(null, null);

        // Verify common containment calls
        assertEquals(1, recyclerView.getItemDecorationCount());
        assertEquals(
                ContainmentItemDecoration.class, recyclerView.getItemDecorationAt(0).getClass());
        assertEquals(expectedAdapter, recyclerView.getAdapter());
    }

    @Test
    public void
            testUpdateFragmentContainment_twoColumn_mainSettings_removesContainmentDecoration() {
        when(mDelegate.isTwoColumnSettingsVisible()).thenReturn(true);

        when(mMainSettings.getContext()).thenReturn(mContext);

        RecyclerView recyclerView = createRecyclerView();
        setFragmentList(mMainSettings, recyclerView);

        PreferenceManager preferenceManager = new PreferenceManager(mContext);
        PreferenceScreen preferenceScreen = preferenceManager.createPreferenceScreen(mContext);
        when(mMainSettings.getPreferenceScreen()).thenReturn(preferenceScreen);

        // First apply single-column containment.
        when(mDelegate.isTwoColumnSettingsVisible()).thenReturn(false);
        mContainmentHelper.updateFragmentContainment(mMainSettings);
        assertEquals(1, recyclerView.getItemDecorationCount());
        assertEquals(
                ContainmentItemDecoration.class, recyclerView.getItemDecorationAt(0).getClass());

        doReturn(mMultiColumnSettings).when(mDelegate).getMultiColumnSettings();
        when(mDelegate.isTwoColumnSettingsVisible()).thenReturn(true);
        mContainmentHelper.updateFragmentContainment(mMainSettings);

        // Verify setMultiColumnSettings was called with non-null SelectionDecoration in two-column
        // mode.
        verify(mMainSettings)
                .setMultiColumnSettings(eq(mMultiColumnSettings), any(SelectionDecoration.class));

        // Verify ContainmentItemDecoration was removed from RecyclerView
        assertEquals(0, recyclerView.getItemDecorationCount());
    }

    /** Creates a no-op {@link RecyclerView} with an adapter for testing. */
    private RecyclerView createRecyclerView() {
        RecyclerView recyclerView = new RecyclerView(mContext);
        RecyclerView.Adapter adapter =
                new RecyclerView.Adapter() {
                    @Override
                    public RecyclerView.ViewHolder onCreateViewHolder(
                            android.view.ViewGroup parent, int viewType) {
                        return null;
                    }

                    @Override
                    public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {}

                    @Override
                    public int getItemCount() {
                        return 0;
                    }
                };
        recyclerView.setAdapter(adapter);
        return recyclerView;
    }

    /**
     * Sets the PreferenceFragmentCompat#mList field for testing using reflection. The field itself
     * is private and the getListView() method is final and cannot be mocked.
     */
    private void setFragmentList(PreferenceFragmentCompat fragment, RecyclerView recyclerView) {
        try {
            java.lang.reflect.Field field =
                    PreferenceFragmentCompat.class.getDeclaredField("mList");
            field.setAccessible(true);
            field.set(fragment, recyclerView);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
}
