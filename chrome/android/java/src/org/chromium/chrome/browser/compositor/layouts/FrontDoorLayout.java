package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import androidx.appcompat.widget.Toolbar;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.scene_layer.TabListSceneLayer;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.shopping_tiles.ShoppingTileSection;
import org.chromium.components.browser_ui.widget.TintedDrawable;

public class FrontDoorLayout extends Layout {
    private SceneLayer mSceneLayer;
    Context mContext;
    Supplier<ShoppingTileSection> mShoppingTileSectionSupplier;
    ViewGroup mFrontDoorLayoutView;
    ObservableSupplier<BrowserControlsManager> mBrowserControlsStateProviderSupplier;

    public FrontDoorLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, Supplier<ShoppingTileSection> shoppingTileSectionSupplier,
            ObservableSupplier<BrowserControlsManager> browserControlsStateProviderSupplier) {
        super(context, updateHost, renderHost);
        mShoppingTileSectionSupplier = shoppingTileSectionSupplier;
        mBrowserControlsStateProviderSupplier = browserControlsStateProviderSupplier;
    }

    @Override
    public void show(long time, boolean animate) {
        super.show(time, animate);
        doneShowing();
    }

    @Override
    public int getLayoutType() {
        // Dummy for now.
        return LayoutType.BROWSING;
    }

    @Override
    public void attachViews(ViewGroup container) {
        if (mFrontDoorLayoutView == null) {
            mFrontDoorLayoutView = (ViewGroup) LayoutInflater.from(container.getContext())
                                           .inflate(R.layout.front_door_main_layout, null);
            adjustForFullscreen();
            Toolbar toolbar = mFrontDoorLayoutView.findViewById(R.id.toolbar);
            TintedDrawable navigationIconDrawable = TintedDrawable.constructTintedDrawable(
                    getContext(), org.chromium.chrome.R.drawable.ic_arrow_back_white_24dp);
            toolbar.setNavigationIcon(navigationIconDrawable);
            toolbar.setNavigationOnClickListener((v) -> { startHiding(mNextTabId, false); });
            toolbar.setTitle("Shopping Front Door");
        }

        if (container == null || mFrontDoorLayoutView.getParent() != null) return;

        ViewGroup overviewList =
                (ViewGroup) container.findViewById(R.id.overview_list_layout_holder);
        overviewList.setVisibility(View.VISIBLE);
        overviewList.addView(mFrontDoorLayoutView);
        //        mFrontDoorLayoutView.addView(mShoppingTileSectionSupplier.get().getTileSectionView());
    }

    private void adjustForFullscreen() {
        if (mFrontDoorLayoutView == null) return;
        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) mFrontDoorLayoutView.getLayoutParams();
        if (params == null) return;

        params.bottomMargin = (int) (getBottomBrowserControlsHeight()
                * mContext.getResources().getDisplayMetrics().density);
        params.topMargin = mBrowserControlsStateProviderSupplier.get().getContentOffset();

        mFrontDoorLayoutView.setLayoutParams(params);
    }

    @Override
    public void detachViews() {
        if (mFrontDoorLayoutView != null) {
            mFrontDoorLayoutView.removeAllViews();
            ViewGroup parent = (ViewGroup) mFrontDoorLayoutView.getParent();
            if (parent != null) {
                parent.setVisibility(View.GONE);
                parent.removeView(mFrontDoorLayoutView);
            }
        }
    }

    @Override
    public @ViewportMode int getViewportMode() {
        return ViewportMode.ALWAYS_FULLSCREEN;
    }

    @Override
    public void onFinishNativeInitialization() {
        mSceneLayer = new TabListSceneLayer();
    }

    @Override
    protected EventFilter getEventFilter() {
        return null;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        return mSceneLayer;
    }
}
