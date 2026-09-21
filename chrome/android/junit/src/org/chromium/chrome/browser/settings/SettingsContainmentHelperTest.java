// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
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
        ArgumentCaptor<FragmentManager.FragmentLifecycleCallbacks> callbackCaptor =
                ArgumentCaptor.forClass(FragmentManager.FragmentLifecycleCallbacks.class);
        verify(mFragmentManager)
                .registerFragmentLifecycleCallbacks(callbackCaptor.capture(), eq(true));
        FragmentManager.FragmentLifecycleCallbacks callbacks = callbackCaptor.getValue();

        TestProviderFragment fragment = new TestProviderFragment();
        callbacks.onFragmentAttached(mFragmentManager, fragment, mContext);

        assertEquals(mObserver, fragment.mObserver);
    }

    @Test
    public void testOnFragmentDetached_removesObserver() {
        mContainmentHelper.registerCallbacks(mFragmentManager);
        ArgumentCaptor<FragmentManager.FragmentLifecycleCallbacks> callbackCaptor =
                ArgumentCaptor.forClass(FragmentManager.FragmentLifecycleCallbacks.class);
        verify(mFragmentManager)
                .registerFragmentLifecycleCallbacks(callbackCaptor.capture(), eq(true));
        FragmentManager.FragmentLifecycleCallbacks callbacks = callbackCaptor.getValue();

        TestProviderFragment fragment = new TestProviderFragment();
        fragment.setPreferenceUpdateObserver(mObserver);

        callbacks.onFragmentDetached(mFragmentManager, fragment);

        assertNull(fragment.mObserver);
    }

    @Test
    public void testOnFragmentViewCreated_addsLayoutListener() {
        mContainmentHelper.registerCallbacks(mFragmentManager);
        ArgumentCaptor<FragmentManager.FragmentLifecycleCallbacks> callbackCaptor =
                ArgumentCaptor.forClass(FragmentManager.FragmentLifecycleCallbacks.class);
        verify(mFragmentManager)
                .registerFragmentLifecycleCallbacks(callbackCaptor.capture(), eq(true));
        FragmentManager.FragmentLifecycleCallbacks callbacks = callbackCaptor.getValue();

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

        ArgumentCaptor<ViewTreeObserver.OnGlobalLayoutListener> listenerCaptor =
                ArgumentCaptor.forClass(ViewTreeObserver.OnGlobalLayoutListener.class);

        // First call registers an initial layout listener.
        mContainmentHelper.postUpdateContainmentOnLayout(mPreferenceFragment);
        verify(mViewTreeObserver).addOnGlobalLayoutListener(listenerCaptor.capture());
        ViewTreeObserver.OnGlobalLayoutListener firstListener = listenerCaptor.getValue();

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

        ArgumentCaptor<ViewTreeObserver.OnGlobalLayoutListener> listenerCaptor =
                ArgumentCaptor.forClass(ViewTreeObserver.OnGlobalLayoutListener.class);

        mContainmentHelper.postUpdateContainmentOnLayout(mPreferenceFragment);
        verify(mViewTreeObserver).addOnGlobalLayoutListener(listenerCaptor.capture());
        ViewTreeObserver.OnGlobalLayoutListener listener = listenerCaptor.getValue();

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

        MainSettings mockMainSettings = mock(MainSettings.class);
        when(mockMainSettings.getContext()).thenReturn(mContext);

        RecyclerView recyclerView = createRecyclerView();
        RecyclerView.Adapter expectedAdapter = recyclerView.getAdapter();
        setFragmentList(mockMainSettings, recyclerView);

        PreferenceManager preferenceManager = new PreferenceManager(mContext);
        PreferenceScreen preferenceScreen = preferenceManager.createPreferenceScreen(mContext);
        when(mockMainSettings.getPreferenceScreen()).thenReturn(preferenceScreen);

        mContainmentHelper.updateFragmentContainment(mockMainSettings);

        // Verify MainSettings specific call
        verify(mockMainSettings).setMultiColumnSettings(null, null);

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

        MainSettings mockMainSettings = mock(MainSettings.class);
        when(mockMainSettings.getContext()).thenReturn(mContext);

        RecyclerView recyclerView = createRecyclerView();
        setFragmentList(mockMainSettings, recyclerView);

        PreferenceManager preferenceManager = new PreferenceManager(mContext);
        PreferenceScreen preferenceScreen = preferenceManager.createPreferenceScreen(mContext);
        when(mockMainSettings.getPreferenceScreen()).thenReturn(preferenceScreen);

        // First apply single-column containment.
        when(mDelegate.isTwoColumnSettingsVisible()).thenReturn(false);
        mContainmentHelper.updateFragmentContainment(mockMainSettings);
        assertEquals(1, recyclerView.getItemDecorationCount());
        assertEquals(
                ContainmentItemDecoration.class, recyclerView.getItemDecorationAt(0).getClass());

        MultiColumnSettings mockMultiColumnSettings = mock(MultiColumnSettings.class);
        doReturn(mockMultiColumnSettings).when(mDelegate).getMultiColumnSettings();
        when(mDelegate.isTwoColumnSettingsVisible()).thenReturn(true);
        mContainmentHelper.updateFragmentContainment(mockMainSettings);

        // Verify setMultiColumnSettings was called with non-null SelectionDecoration in two-column
        // mode.
        verify(mockMainSettings)
                .setMultiColumnSettings(
                        eq(mockMultiColumnSettings), any(SelectionDecoration.class));

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
