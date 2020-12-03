package org.chromium.chrome.browser.shopping_tiles;

// Information about one product.
public class ProductInfo {
    public String name;
    public float price;
    public String productUrl;
    public String imageUrl;
    public String priceStr;
    public boolean isRecentlyView;

    private ProductInfo(
            String name, float price, String productUrl, String imageUrl, boolean isRecentlyView) {
        this.name = name;
        this.price = price;
        this.productUrl = productUrl;
        this.imageUrl = imageUrl;
        this.isRecentlyView = isRecentlyView;
    }

    private ProductInfo(
            String name, String price, String productUrl, String imageUrl, boolean isRecentlyView) {
        this.name = name;
        this.priceStr = price;
        this.productUrl = productUrl;
        this.imageUrl = imageUrl;
        this.isRecentlyView = isRecentlyView;
    }

    public static class Builder {
        String mName;
        float mPrice;
        String mPriceStr;
        String mUrl;
        String mImageUrl;
        boolean mIsRecentlyView;

        Builder withName(String name) {
            mName = name;
            return this;
        }

        Builder withPrice(float price) {
            mPrice = price;
            return this;
        }

        Builder withUrl(String url) {
            mUrl = url;
            return this;
        }

        Builder withImageUrl(String url) {
            mImageUrl = url;
            return this;
        }

        Builder withPriceStr(String price) {
            mPriceStr = price;
            return this;
        }

        Builder withRecentlyView(boolean isRecentlyView) {
            mIsRecentlyView = isRecentlyView;
            return this;
        }

        ProductInfo build() {
            if (mPriceStr == null) {
                return new ProductInfo(mName, mPrice, mUrl, mImageUrl, mIsRecentlyView);
            }
            return new ProductInfo(mName, mPriceStr, mUrl, mImageUrl, mIsRecentlyView);
        }
    }
}