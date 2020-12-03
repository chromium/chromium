package org.chromium.chrome.browser.shopping_tiles;

import org.chromium.components.browser_ui.widget.image_tiles.ImageTile;

public class ShoppingTile extends ImageTile {
    private final String mTileUrl;
    public final boolean mIsMerchantTile;
    public final String mMerchant;
    public final String mCategory;
    public final String mImageUrl;

    /**
     * Constructor.
     *
     * @param id
     * @param displayTitle
     * @param accessibilityText
     */
    public ShoppingTile(String id, String displayTitle, String accessibilityText, String merchant,
            String category, boolean isMerchantTile, String imageUrl) {
        super(id, displayTitle, accessibilityText);
        mIsMerchantTile = isMerchantTile;
        if (mIsMerchantTile) {
            mTileUrl = null;
        } else {
            mTileUrl = "http://google.com/search?tbm=shop&q=" + merchant + "+" + category;
        }
        mMerchant = merchant;
        mCategory = category;
        mImageUrl = imageUrl;
    }

    public String getTileUrl() {
        return mTileUrl;
    }
}
