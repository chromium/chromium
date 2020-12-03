package org.chromium.chrome.browser.shopping_tiles;

import android.graphics.Bitmap;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.Button;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTile;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTileCoordinator;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTileCoordinatorFactory;
import org.chromium.components.browser_ui.widget.image_tiles.TileConfig;
import org.chromium.components.query_tiles.QueryTileConstants;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class ShoppingTileSection {
    private final ViewGroup mShoppingTileSectionView;
    private final ShoppingTileProvider mTileProvider;
    private final Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private final ImageFetcher mImageFetcher;
    private final Button mMoreButton;
    private ImageTileCoordinator mTileCoordinator;
    private Integer mTileWidth;
    private Supplier<OverviewModeBehavior> mOverviewModeBehaviorSupplier;

    private ShoppingTasksSection mTasksSection;

    class ShoppingTileProvider {
        void getTiles(Callback<List<ShoppingTile>> callback) {
            List<ShoppingTile> tiles = new ArrayList<>();
            ShoppingTile merchantTile = new ShoppingTile("Sephora", "Sephora", "Sephora", "Sephora",
                    "", true,
                    "https://i.pinimg.com/280x280_RS/88/20/0a/88200abdce42bf6ad8f36dd62760d1c9.jpg");
            ShoppingTile lipsStick = new ShoppingTile("Lipstick", "Lipstick", "Lipstick", "Sephora",
                    "Lipstick", false,
                    "https://www.sephora.com/productimages/sku/s2198661-main-hero.jpg");
            ShoppingTile Eyeshadow = new ShoppingTile("Eyeshadow", "Eyeshadow", "Eyeshadow",
                    "Sephora", "Eyeshadow", false,
                    "https://www.sephora.com/productimages/sku/s2079085-main-zoom.jpg?pb=2020-03-sephora-value-2019&imwidth=300");
            ShoppingTile Foundation = new ShoppingTile("Foundation", "Foundation", "Foundation",
                    "Sephora", "Foundation", false,
                    "https://www.sephora.com/productimages/sku/s1787589-main-zoom.jpg?pb=2020-03-sephora-value-2019&imwidth=300");
            tiles.add(merchantTile);
            tiles.add(lipsStick);
            tiles.add(Eyeshadow);
            tiles.add(Foundation);
            callback.onResult(tiles);
        }
    }

    public ShoppingTileSection(ViewStub shoppingTileSectionViewStub,
            Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier, Profile profile,
            Supplier<OverviewModeBehavior> overviewModeBehaviorSupplier) {
        shoppingTileSectionViewStub.setLayoutResource(R.layout.shopping_tiles);
        mShoppingTileSectionView = (ViewGroup) shoppingTileSectionViewStub.inflate();
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mOverviewModeBehaviorSupplier = overviewModeBehaviorSupplier;

        mTileProvider = new ShoppingTileProvider();
        mImageFetcher = ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.NETWORK_ONLY,
                profile, GlobalDiscardableReferencePool.getReferencePool());
        mMoreButton = mShoppingTileSectionView.findViewById(R.id.more_button);
        mMoreButton.setOnClickListener(this::onMoreButtonClicked);

        // Shopping tile
        TileConfig tileConfig = new TileConfig.Builder().setUmaPrefix("Shopping").build();
        mTileCoordinator = ImageTileCoordinatorFactory.create(mShoppingTileSectionView.getContext(),
                tileConfig, this::onTileClicked, this::getVisuals);
        mShoppingTileSectionView.addView(mTileCoordinator.getView(),
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        // Shopping task
        mTasksSection = new ShoppingTasksSection(
                mShoppingTileSectionView.getContext(), mEphemeralTabCoordinatorSupplier, profile);
        mShoppingTileSectionView.addView(mTasksSection.getView());
        reloadTiles();
    }

    /**
     * Called to clear the state and reload the top level tiles. Any chip selected will be cleared.
     */
    public void reloadTiles() {
        mTileProvider.getTiles(this::setTiles);
    }

    private void onTileClicked(ImageTile tile) {
        // show emphameral tab with google shopping data.
        ShoppingTile shoppingTile = (ShoppingTile) tile;
        Log.d("Meil", "Click tile-----" + ((ShoppingTile) tile).getTileUrl());
        if (shoppingTile.mIsMerchantTile) {
            mEphemeralTabCoordinatorSupplier.get().requestOpenSheet(
                    "https://www.sephora.com", "Sephora", false);
        } else {
            mEphemeralTabCoordinatorSupplier.get().requestOpenSheet(shoppingTile.getTileUrl(),
                    shoppingTile.mMerchant + " " + shoppingTile.mCategory, false);
        }
    }

    private void onMoreButtonClicked(View view) {
        // show front door
        showFrontDoor();
    }

    private void showFrontDoor() {
        mOverviewModeBehaviorSupplier.get().showFrontDoor();
    }

    private void getVisuals(ImageTile tile, Callback<List<Bitmap>> callback) {
        // TODO(crbug.com/1077086): Probably need a bigger width to start with or pass the exact
        // width. Also may need to update on orientation change.
        if (mTileWidth == null) {
            mTileWidth = mShoppingTileSectionView.getResources().getDimensionPixelSize(
                    R.dimen.tile_ideal_width);
        }

        fetchImage((ShoppingTile) tile, mTileWidth,
                bitmap -> { callback.onResult(Arrays.asList(bitmap)); });
    }

    private void fetchImage(ShoppingTile shoppingTile, int size, Callback<Bitmap> callback) {
        // Fetch Merchant image and category image.
        ImageFetcher.Params params = ImageFetcher.Params.createWithExpirationInterval(
                shoppingTile.mImageUrl, ImageFetcher.SHOPPING_TILE_UMA_CLIENT_NAME, size, size,
                QueryTileConstants.IMAGE_EXPIRATION_INTERVAL_MINUTES);
        mImageFetcher.fetchImage(params, callback);
    }

    private void setTiles(List<ShoppingTile> tiles) {
        mTileCoordinator.setTiles(new ArrayList<>(tiles));
        mShoppingTileSectionView.setVisibility(View.VISIBLE);
    }

    public View getTileSectionView() {
        return mTileCoordinator.getView();
    }
}
