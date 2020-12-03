package org.chromium.chrome.browser.shopping.front_door;

import java.util.ArrayList;
import java.util.List;

// Representing a ProductLine.
public class ProductLineInfo {
    public String name;
    public String brand;
    public String brandUrl;
    public String brandMid;
    public String imageUrl;
    public String clickingUrl;
    public List<String> categoryKeyList;

    private ProductLineInfo(String name, String brand, String brandMid, String brandUrl,
            String imageUrl, String clickingUrl, List<String> categoryKeyList) {
        this.name = name;
        this.brand = brand;
        this.brandMid = brandMid;
        this.brandUrl = brandUrl;
        this.imageUrl = imageUrl;
        this.clickingUrl = clickingUrl;
        this.categoryKeyList = new ArrayList<>(categoryKeyList);
    }

    public static class Builder {
        String mName;
        String mBrand;
        String mBrandMid;
        String mBrandUrl;
        String mUrl;
        String mImageUrl;
        List<String> mCategoryKeyList;

        ProductLineInfo.Builder withProductLineName(String name) {
            mName = name;
            return this;
        }

        ProductLineInfo.Builder withBrand(String brand) {
            mBrand = brand;
            return this;
        }

        ProductLineInfo.Builder withBrandMid(String brandMid) {
            mBrandMid = brandMid;
            return this;
        }

        ProductLineInfo.Builder withBrandUrl(String brand) {
            mBrandUrl = brand;
            return this;
        }

        ProductLineInfo.Builder withClickingUrl(String url) {
            mUrl = url;
            return this;
        }

        ProductLineInfo.Builder withImageUrl(String url) {
            mImageUrl = url;
            return this;
        }

        ProductLineInfo.Builder withCategoryKeyList(List<String> keys) {
            mCategoryKeyList = keys;
            return this;
        }

        public ProductLineInfo build() {
            return new ProductLineInfo(
                    mName, mBrand, mBrandMid, mBrandUrl, mImageUrl, mUrl, mCategoryKeyList);
        }
    }
}
