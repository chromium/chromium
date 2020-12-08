package org.chromium.chrome.browser.shopping.front_door;

import android.text.TextUtils;

import java.util.ArrayList;
import java.util.List;

// Representing a Brand
public class BrandInfo {
    public String name;
    public String id;
    public String url;
    public String logoUrl;
    public List<String> imageUrls;

    private BrandInfo(String name, String id, String url, List<String> imageUrls, String logoUrl) {
        this.name = name;
        this.id = id;
        this.url = url;
        this.imageUrls = new ArrayList<>(imageUrls);
        this.logoUrl = TextUtils.isEmpty(logoUrl) ? "" : logoUrl;
    }

    public static class Builder {
        private String mName;
        private String mId;
        private String mUrl;
        private List<String> mImageUrls;
        private String mLogoUrl;

        BrandInfo.Builder withName(String name) {
            this.mName = name;
            return this;
        }

        BrandInfo.Builder withId(String id) {
            this.mId = id;
            return this;
        }

        BrandInfo.Builder withUrl(String url) {
            this.mUrl = url;
            return this;
        }

        BrandInfo.Builder withLogoUrl(String logoUrl) {
            this.mLogoUrl = logoUrl;
            return this;
        }

        BrandInfo.Builder withImageUrls(List<String> urls) {
            this.mImageUrls = urls;
            return this;
        }

        public BrandInfo build() {
            return new BrandInfo(mName, mId, mUrl, mImageUrls, mLogoUrl);
        }
    }
}
