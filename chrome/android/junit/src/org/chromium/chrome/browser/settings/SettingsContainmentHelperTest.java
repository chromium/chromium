// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Bundle;
import android.view.View;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceGroup.PreferencePositionCallback;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.PreferenceUpdateObserver;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemDecoration;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link SettingsContainmentHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsContainmentHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SettingsContainmentHelper.Delegate mDelegate;
    @Mock private FragmentManager mFragmentManager;
    @Mock private PreferenceUpdateObserver mObserver;

    @Mock private PreferenceFragmentCompat mPreferenceFragment;

    private Context mContext;
    private View mView;
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
        mView = new View(mContext);
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

        RecyclerView recyclerView = setUpPreferenceFragmentWithList();

        callbacks.onFragmentViewCreated(mFragmentManager, mPreferenceFragment, mView, null);

        // Containment is only applied once a global layout pass happens.
        assertEquals(0, recyclerView.getItemDecorationCount());
        mView.getViewTreeObserver().dispatchOnGlobalLayout();
        assertEquals(1, recyclerView.getItemDecorationCount());
    }

    @Test
    public void testPostUpdateContainmentOnLayout_addsLayoutListener() {
        RecyclerView recyclerView = setUpPreferenceFragmentWithList();

        mContainmentHelper.postUpdateContainmentOnLayout(mPreferenceFragment);

        // Containment is only applied once a global layout pass happens.
        assertEquals(0, recyclerView.getItemDecorationCount());
        mView.getViewTreeObserver().dispatchOnGlobalLayout();
        assertEquals(1, recyclerView.getItemDecorationCount());
    }

    @Test
    public void testPostUpdateContainmentOnLayout_removesExistingListenerBeforeAddingNew() {
        setUpPreferenceFragmentWithList();

        // First call registers an initial layout listener.
        mContainmentHelper.postUpdateContainmentOnLayout(mPreferenceFragment);

        // Second call must unregister the previous listener before registering a new one
        // to prevent duplicate triggers and listener leaks on rapid successive updates.
        mContainmentHelper.postUpdateContainmentOnLayout(mPreferenceFragment);
        mView.getViewTreeObserver().dispatchOnGlobalLayout();

        // Containment is updated only once, i.e. only one listener was triggered.
        verify(mPreferenceFragment, times(1)).getPreferenceScreen();
    }

    @Test
    public void testPostUpdateContainmentOnLayout_onGlobalLayoutTriggersUpdateAndCleansUp() {
        RecyclerView recyclerView = setUpPreferenceFragmentWithList();
        RecyclerView.Adapter expectedAdapter = recyclerView.getAdapter();

        mContainmentHelper.postUpdateContainmentOnLayout(mPreferenceFragment);

        // Simulate global layout completion passes.
        mView.getViewTreeObserver().dispatchOnGlobalLayout();
        mView.getViewTreeObserver().dispatchOnGlobalLayout();

        // Verify listener removes itself to prevent redundant future invocations,
        // attaches ContainmentItemDecoration, and preserves adapter without view re-inflation.
        verify(mPreferenceFragment, times(1)).getPreferenceScreen();
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

    private abstract static class TestPreferenceAdapter
            extends RecyclerView.Adapter<RecyclerView.ViewHolder>
            implements PreferencePositionCallback {}

    @Test
    public void testOnFragmentAttached_scrollsAndHighlightsPreferenceFromArgs() {
        mContainmentHelper.registerCallbacks(mFragmentManager);
        ArgumentCaptor<FragmentManager.FragmentLifecycleCallbacks> callbackCaptor =
                ArgumentCaptor.forClass(FragmentManager.FragmentLifecycleCallbacks.class);
        verify(mFragmentManager)
                .registerFragmentLifecycleCallbacks(callbackCaptor.capture(), eq(true));
        FragmentManager.FragmentLifecycleCallbacks callbacks = callbackCaptor.getValue();

        PreferenceFragmentCompat fragment =
                spy(
                        new PreferenceFragmentCompat() {
                            @Override
                            public void onCreatePreferences(
                                    @Nullable Bundle savedInstanceState,
                                    @Nullable String rootKey) {}
                        });
        doNothing().when(fragment).scrollToPreference("my_pref");
        when(fragment.getView()).thenReturn(mView);

        View itemView = new View(mContext);
        RecyclerView.ViewHolder viewHolder = new RecyclerView.ViewHolder(itemView) {};
        RecyclerView listView =
                new RecyclerView(mContext) {
                    @Override
                    public @Nullable ViewHolder findViewHolderForAdapterPosition(int position) {
                        return position == 2 ? viewHolder : null;
                    }
                };
        TestPreferenceAdapter mockAdapter = mock(TestPreferenceAdapter.class);
        when(mockAdapter.getPreferenceAdapterPosition("my_pref")).thenReturn(2);
        listView.setAdapter(mockAdapter);
        setFragmentList(fragment, listView);

        Bundle args = SettingsNavigation.createHighlightArgs(null, "my_pref");
        fragment.setArguments(args);

        callbacks.onFragmentAttached(mFragmentManager, fragment, mContext);

        assertFalse(args.containsKey(SettingsNavigation.EXTRA_HIGHLIGHT_PREFERENCE));

        ShadowLooper.idleMainLooper(250, TimeUnit.MILLISECONDS);

        verify(fragment).scrollToPreference("my_pref");
        assertTrue((Boolean) itemView.getTag(R.id.highlight_state));
    }

    /**
     * Sets up {@link #mPreferenceFragment} with {@link #mView} as its view and an empty preference
     * list.
     *
     * @return The fragment's list {@link RecyclerView}.
     */
    private RecyclerView setUpPreferenceFragmentWithList() {
        when(mPreferenceFragment.getView()).thenReturn(mView);
        when(mPreferenceFragment.getContext()).thenReturn(mContext);

        RecyclerView recyclerView = createRecyclerView();
        setFragmentList(mPreferenceFragment, recyclerView);

        PreferenceManager preferenceManager = new PreferenceManager(mContext);
        PreferenceScreen preferenceScreen = preferenceManager.createPreferenceScreen(mContext);
        when(mPreferenceFragment.getPreferenceScreen()).thenReturn(preferenceScreen);
        return recyclerView;
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
