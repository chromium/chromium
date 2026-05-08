// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.appcompat.content.res.AppCompatResources;

import com.airbnb.lottie.LottieCompositionFactory;
import com.airbnb.lottie.LottieDrawable;
import com.airbnb.lottie.LottieProperty;
import com.airbnb.lottie.model.KeyPath;
import com.airbnb.lottie.value.LottieValueCallback;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Set;
import java.util.function.Supplier;

/** Defines a toolbar button to open the Glic bottom sheet. */
@NullMarked
public class GlicToolbarButtonController extends BaseButtonDataProvider
        implements ActorKeyedService.Observer, GlicKeyedService.GlobalShowHideObserver {
    public static final int ACTION_CHIP_COLLAPSE_DELAY_MS = 30000;

    @IntDef({ButtonState.DEFAULT, ButtonState.WORKING, ButtonState.NEEDS_REVIEW, ButtonState.DONE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ButtonState {
        int DEFAULT = 0;
        int WORKING = 1;
        int NEEDS_REVIEW = 2;
        int DONE = 3;
    }

    /** Delegate interface for handling clicks on the Glic toolbar button. */
    @FunctionalInterface
    public interface GlicButtonDelegate {
        /**
         * Called when the Glic button is clicked.
         *
         * @param preventClose whether to prevent closing the Glic UI if it's already open.
         */
        void onClick(boolean preventClose);
    }

    private final Activity mActivity;
    private final GlicButtonDelegate mToggleGlicCallback;
    private final Supplier<@Nullable Tracker> mTrackerSupplier;
    private final Supplier<@Nullable ChromeAndroidTask> mTaskSupplier;
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final Supplier<@Nullable TabModelSelector> mTabModelSelectorSupplier;
    private @Nullable Profile mCurrentProfile;
    private @Nullable ActorKeyedService mCurrentActorService;
    private @Nullable GlicKeyedService mCurrentGlicService;
    private final ButtonSpec mDefaultSpec;
    private final ButtonSpec mWorkingSpec;
    private final ButtonSpec mReviewSpec;
    private final ButtonSpec mDoneSpec;

    private @ButtonState int mButtonState = ButtonState.DEFAULT;
    private boolean mPersistDoneState;
    private boolean mIsPanelOpen;
    private int mBrowserControlsShowingToken = TokenHolder.INVALID_TOKEN;
    private @Nullable AnchoredPopupWindow mMenuWindow;

    /**
     * @param activity The Android activity.
     * @param activeTabSupplier The currently active tab.
     * @param toggleGlicCallback Callback to run when the button is clicked to open Glic.
     * @param trackerSupplier Supplier for the current profile tracker.
     * @param taskSupplier Supplier for the ChromeAndroidTask.
     * @param browserControlsVisibilityManager Manager for browser controls.
     * @param tabModelSelectorSupplier Supplier for the TabModelSelector.
     */
    public GlicToolbarButtonController(
            Activity activity,
            Supplier<@Nullable Tab> activeTabSupplier,
            GlicButtonDelegate toggleGlicCallback,
            Supplier<@Nullable Tracker> trackerSupplier,
            Supplier<@Nullable ChromeAndroidTask> taskSupplier,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier) {
        // TODO(crbug.com/482372270): Add correct styling to button including Nudge state text,
        // active state shape change, and appropriate colors.
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                new ButtonSpec.Builder(
                                AppCompatResources.getDrawable(activity, R.drawable.ic_spark_24dp),
                                activity.getString(
                                        R.string.glic_button_entrypoint_ask_gemini_label),
                                /* supportsTinting= */ true)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.GLIC)
                        .build());
        mActivity = activity;
        mToggleGlicCallback = toggleGlicCallback;
        mTrackerSupplier = trackerSupplier;
        mTaskSupplier = taskSupplier;
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mDefaultSpec = mButtonData.getButtonSpec();
        Drawable collapsedDrawable =
                AppCompatResources.getDrawable(activity, R.drawable.glic_dirty_dot_spark);
        mWorkingSpec = createWorkingSpec(activity);
        mReviewSpec =
                new ButtonSpec.Builder(createReviewSpec())
                        .setCollapsedDrawable(collapsedDrawable)
                        .build();
        mDoneSpec =
                new ButtonSpec.Builder(createDoneSpec())
                        .setCollapsedDrawable(collapsedDrawable)
                        .build();
    }

    private ButtonSpec createReviewSpec() {
        return new ButtonSpec.Builder(mDefaultSpec)
                .setActionChipLabelResId(R.string.glic_button_status_review)
                .setShouldSuppressCpa(true)
                .setActionChipCollapseDelayMs(ACTION_CHIP_COLLAPSE_DELAY_MS)
                .build();
    }

    private ButtonSpec createDoneSpec() {
        return new ButtonSpec.Builder(mDefaultSpec)
                .setActionChipLabelResId(R.string.glic_button_status_done)
                .setShouldSuppressCpa(true)
                .setActionChipCollapseDelayMs(ACTION_CHIP_COLLAPSE_DELAY_MS)
                .build();
    }

    private ButtonSpec createWorkingSpec(Context context) {
        LottieDrawable lottieDrawable = new LottieDrawable();
        LottieCompositionFactory.fromRawRes(context, R.raw.glic_spinner)
                .addListener(
                        composition -> {
                            lottieDrawable.setComposition(composition);
                            lottieDrawable.setRepeatCount(LottieDrawable.INFINITE);
                            lottieDrawable.playAnimation();
                        });
        Drawable sparkIcon = AppCompatResources.getDrawable(context, R.drawable.ic_spark_24dp);
        LayerDrawable layerDrawable =
                new LayerDrawable(new Drawable[] {lottieDrawable, sparkIcon}) {
                    private @Nullable ColorStateList mTintList;

                    @Override
                    public void setTintList(@Nullable ColorStateList tint) {
                        super.setTintList(tint);
                        mTintList = tint;
                        updateLottieTint();
                    }

                    @Override
                    protected boolean onStateChange(int[] state) {
                        boolean changed = super.onStateChange(state);
                        if (updateLottieTint()) {
                            changed = true;
                        }
                        return changed;
                    }

                    @Override
                    public boolean isStateful() {
                        return (mTintList != null && mTintList.isStateful()) || super.isStateful();
                    }

                    private boolean updateLottieTint() {
                        if (mTintList != null) {
                            int color =
                                    mTintList.getColorForState(
                                            getState(), mTintList.getDefaultColor());
                            lottieDrawable.addValueCallback(
                                    new KeyPath("**"),
                                    LottieProperty.COLOR_FILTER,
                                    new LottieValueCallback<>(
                                            new PorterDuffColorFilter(
                                                    color, PorterDuff.Mode.SRC_IN)));
                            return true;
                        }
                        return false;
                    }
                };
        float density = context.getResources().getDisplayMetrics().density;
        // Adjust sizes of the spark and spinner.
        int sparkInset = Math.round(2 * density);
        int spinnerInset = Math.round(-10 * density);
        layerDrawable.setLayerInset(0, spinnerInset, spinnerInset, spinnerInset, spinnerInset);
        layerDrawable.setLayerInset(1, sparkInset, sparkInset, sparkInset, sparkInset);
        return new ButtonSpec.Builder(mDefaultSpec)
                .setDrawable(layerDrawable)
                .setShouldSuppressCpa(true)
                .build();
    }

    @Override
    protected boolean shouldShowButton(@Nullable Tab tab) {
        if (tab == null || tab.isOffTheRecord() || UrlUtilities.isNtpUrl(tab.getUrl())) {
            return false;
        }
        // TODO(crbug.com/499354469): Add proper checks for glic availability.
        if (!AdaptiveToolbarFeatures.isGlicEnabledForProfile(tab.getProfile())) {
            return false;
        }
        return super.shouldShowButton(tab);
    }

    @Override
    public ButtonData get(@Nullable Tab tab) {
        ButtonData buttonData = super.get(tab);
        if (!buttonData.canShow()) {
            return buttonData;
        }

        // This can be assumed because shouldShowButton hides the entrypoint if there's no tab.
        assumeNonNull(tab);
        updateObservations(tab.getProfile());
        updateButtonState();

        ButtonSpec desiredSpec = mDefaultSpec;
        switch (mButtonState) {
            case ButtonState.NEEDS_REVIEW:
                desiredSpec = mReviewSpec;
                break;
            case ButtonState.WORKING:
                desiredSpec = mWorkingSpec;
                break;
            case ButtonState.DONE:
                desiredSpec = mDoneSpec;
                break;
            case ButtonState.DEFAULT:
            default:
                desiredSpec = mDefaultSpec;
        }
        mButtonData.setButtonSpec(
                new ButtonSpec.Builder(desiredSpec).setIsChecked(mIsPanelOpen).build());

        mButtonData.setEnabled(true);
        return buttonData;
    }

    private void updateButtonState() {
        if (mCurrentActorService == null) {
            updateButtonStateAndControls(ButtonState.DEFAULT);
            return;
        }

        ActorTask task = mCurrentActorService.getCurrentActiveTask();
        int newButtonState;
        if (task != null) {
            newButtonState = mapTaskStateToButtonState(task.getState());
        } else if (mPersistDoneState) {
            newButtonState = ButtonState.DONE;
        } else {
            newButtonState = ButtonState.DEFAULT;
        }

        updateButtonStateAndControls(newButtonState);
    }

    private void updateButtonStateAndControls(int newButtonState) {
        int oldButtonState = mButtonState;
        mButtonState = newButtonState;
        mPersistDoneState = (mButtonState == ButtonState.DONE);

        if (mButtonState != oldButtonState) {
            if (mButtonState == ButtonState.WORKING) {
                acquireBrowserControls();
            } else if (oldButtonState == ButtonState.WORKING) {
                releaseBrowserControls();
            }
        }
    }

    private void acquireBrowserControls() {
        if (mBrowserControlsShowingToken == TokenHolder.INVALID_TOKEN) {
            mBrowserControlsShowingToken =
                    mBrowserControlsVisibilityManager
                            .getBrowserVisibilityDelegate()
                            .showControlsPersistent();
        }
    }

    private void releaseBrowserControls() {
        if (mBrowserControlsShowingToken != TokenHolder.INVALID_TOKEN) {
            mBrowserControlsVisibilityManager
                    .getBrowserVisibilityDelegate()
                    .releasePersistentShowingToken(mBrowserControlsShowingToken);
            mBrowserControlsShowingToken = TokenHolder.INVALID_TOKEN;
        }
    }

    private void updateIsPanelOpen() {
        if (mCurrentGlicService == null || mCurrentProfile == null) return;
        ChromeAndroidTask task = mTaskSupplier.get();
        if (task == null) return;

        long browserWindowPtr = task.getNativeBrowserWindowPtr(mCurrentProfile, mActivity);
        boolean isOpen = false;
        if (browserWindowPtr != 0) {
            isOpen = mCurrentGlicService.isPanelShowingForBrowser(browserWindowPtr);
        }
        if (mIsPanelOpen != isOpen) {
            mIsPanelOpen = isOpen;
            notifyObservers(true);
        }
    }

    private @ButtonState int mapTaskStateToButtonState(@ActorTaskState int taskState) {
        switch (taskState) {
            case ActorTaskState.WAITING_ON_USER:
            case ActorTaskState.FAILED:
                return ButtonState.NEEDS_REVIEW;
            case ActorTaskState.FINISHED:
                return ButtonState.DONE;
            case ActorTaskState.ACTING:
            case ActorTaskState.REFLECTING:
                return ButtonState.WORKING;
            case ActorTaskState.CANCELLED:
            case ActorTaskState.CREATED:
            case ActorTaskState.PAUSED_BY_USER:
            case ActorTaskState.PAUSED_BY_ACTOR:
                return ButtonState.DEFAULT;
            default:
                throw new AssertionError("Unexpected task state: " + taskState);
        }
    }

    private void updateObservations(Profile profile) {
        assert !profile.isOffTheRecord();
        if (profile.equals(mCurrentProfile)) return;

        if (mCurrentActorService != null) {
            mCurrentActorService.removeObserver(this);
        }
        if (mCurrentGlicService != null) {
            mCurrentGlicService.removeGlobalShowHideObserver(this);
        }

        mCurrentProfile = profile;
        mCurrentActorService = ActorKeyedServiceFactory.getForProfile(profile);
        mCurrentGlicService = GlicKeyedServiceFactory.getForProfile(profile);

        if (mCurrentActorService != null) {
            mCurrentActorService.addObserver(this);
        }
        if (mCurrentGlicService != null) {
            mCurrentGlicService.addGlobalShowHideObserver(this);
            updateIsPanelOpen();
        }
    }

    @Override
    public void destroy() {
        if (mCurrentActorService != null) {
            mCurrentActorService.removeObserver(this);
            mCurrentActorService = null;
        }
        if (mCurrentGlicService != null) {
            mCurrentGlicService.removeGlobalShowHideObserver(this);
            mCurrentGlicService = null;
        }
        mCurrentProfile = null;
        super.destroy();
    }

    private void showTaskMenu(View anchorView, List<ActorTask> tasks) {
        ModelList modelList = new ModelList();
        int endIconWidthPx =
                anchorView
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.glic_menu_dot_width);

        // TODO(crbug.com/498721993): Listen to the task and update menu item when needed.
        for (ActorTask task : tasks) {
            ListItemBuilder builder =
                    new ListItemBuilder()
                            .withTitle(task.getTitle())
                            .withIsIncognito(false)
                            .withIsTextEllipsizedAtEnd(true)
                            .withClickListener(
                                    v -> {
                                        switchToActuatingTab(task.getLastActedTabs());
                                        mToggleGlicCallback.onClick(true);
                                        dismissMenu();
                                    });

            if (mapTaskStateToButtonState(task.getState()) == ButtonState.NEEDS_REVIEW) {
                builder.withStartIconRes(R.drawable.ic_hourglass_empty_24dp)
                        .withEndIconRes(R.drawable.glic_menu_dot)
                        .withEndIconWidth(endIconWidthPx);
            } else {
                builder.withStartIconRes(R.drawable.ic_arrow_selector_spark_24dp);
            }

            modelList.add(builder.build());
        }

        // Divider
        modelList.add(BasicListMenu.buildMenuDivider(false));

        // Item 2: Ask Gemini
        modelList.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.glic_button_entrypoint_ask_gemini_label)
                        .withStartIconRes(R.drawable.ic_spark_24dp)
                        .withIsIncognito(false)
                        .withClickListener(
                                v -> {
                                    mToggleGlicCallback.onClick(false);
                                    dismissMenu();
                                })
                        .build());

        ListMenu.Delegate delegate =
                new ListMenu.Delegate() {
                    @Override
                    public void onItemSelected(PropertyModel model, View view) {
                        View.OnClickListener listener =
                                model.get(ListMenuItemProperties.CLICK_LISTENER);

                        if (listener != null) {
                            listener.onClick(view);
                        }
                    }
                };

        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        anchorView.getContext(), modelList, delegate);
        View contentView = listMenu.getContentView();

        // Add gap to the right of the menu so it is not at the right edge of the screen.
        ViewRectProvider anchorRectProvider = new ViewRectProvider(anchorView);
        int endOffsetPx =
                anchorView
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.glic_task_menu_end_offset);

        int lateralPadding = contentView.getPaddingLeft() + contentView.getPaddingRight();
        int widthPx = listMenu.getMaxItemWidth() + lateralPadding;

        int maxWidthPx =
                anchorView
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.glic_task_menu_max_width);
        widthPx = Math.min(widthPx, maxWidthPx);

        AnchoredPopupWindow.Builder builder =
                new AnchoredPopupWindow.Builder(
                                anchorView.getContext(),
                                anchorView.getRootView(),
                                new ColorDrawable(Color.TRANSPARENT),
                                () -> contentView,
                                anchorRectProvider)
                        .setFocusable(true)
                        .setTouchModal(true)
                        .setDismissOnTouchInteraction(true)
                        .setHorizontalOverlapAnchor(true)
                        .setVerticalOverlapAnchor(false)
                        .setPreferredHorizontalOrientation(
                                AnchoredPopupWindow.HorizontalOrientation.LAYOUT_DIRECTION)
                        .setDesiredContentWidth(widthPx)
                        .setMaxWidth(maxWidthPx)
                        .setMargin(endOffsetPx)
                        .setAnimateFromAnchor(true)
                        .setAllowNonTouchableSize(true);
        mMenuWindow = builder.build();
        mMenuWindow.show();
    }

    private void dismissMenu() {
        if (mMenuWindow != null) {
            mMenuWindow.dismiss();
            mMenuWindow = null;
        }
    }

    private void switchToActuatingTab(Set<Integer> tabs) {
        if (!tabs.isEmpty()) {
            int tabId = tabs.iterator().next();
            TabModelSelector selector = mTabModelSelectorSupplier.get();
            if (selector != null) {
                TabModelUtils.selectTabById(selector, tabId, TabSelectionType.FROM_USER);
            }
        }
    }

    @Override
    protected @Nullable IphCommandBuilder getIphCommandBuilder(Tab tab) {
        return new IphCommandBuilder(
                mActivity.getResources(),
                FeatureConstants.GLIC_PROMO_ANDROID_FEATURE,
                R.string.iph_glic_promo_text,
                R.string.iph_glic_promo_accessibility_text);
    }

    @Override
    public void onClick(View view) {
        mPersistDoneState = false;

        if (mMenuWindow != null && mMenuWindow.isShowing()) {
            dismissMenu();
            return;
        }

        if (mCurrentActorService != null) {
            List<ActorTask> tasks = mCurrentActorService.getActiveTasks();
            Tab currentTab = mActiveTabSupplier.get();
            boolean isOnActingTab =
                    currentTab != null
                            && mCurrentActorService.getActiveTaskIdOnTab(currentTab.getId())
                                    != null;

            if (!isOnActingTab && !tasks.isEmpty()) {
                showTaskMenu(view, tasks);
                return;
            }
        }

        mToggleGlicCallback.onClick(false);
        Tracker tracker = mTrackerSupplier.get();
        if (tracker != null) {
            tracker.notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_GLIC_CLICKED);
        }
        updateButtonState();
        notifyObservers(true);
    }

    @Override
    public void onTaskStateChanged(int taskId, @ActorTaskState int newState) {
        if (newState == ActorTaskState.FINISHED) {
            mPersistDoneState = true;
        }
        int oldButtonState = mButtonState;
        updateButtonStateAndControls(mapTaskStateToButtonState(newState));

        if (mButtonState != oldButtonState) {
            notifyObservers(true);
        }
    }

    @Override
    public void onGlobalShowHide() {
        updateIsPanelOpen();
    }
}
