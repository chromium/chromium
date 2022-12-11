package com.ark.browser.utils;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import com.ark.browser.ui.drawable.CircleDrawable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.url.GURL;
import org.chromium.chrome.R;

public class FaviconUtil {

    //    private static final int FAVICON_CORNER_RADIUS_DP = 8;
//    private static final int FAVICON_PADDING_DP = 4;
    private static final int FAVICON_TEXT_SIZE_DP = 10;
    //    private static final int FAVICON_BACKGROUND_COLOR = 0xff969696;
    private static final String[] COLORS = {"#EF5742", "#4A81FF", "#63ADF8",
            "#FF9800", "#9C27B0", "#2196F3", "#5AB963", "#295E62", "#263238",
            "#3E2723", "#212121", "#009688", "#969696"};

    private final Context mContext;
    //    private final int mFaviconSizePx;
    private final FaviconHelper mFaviconHelper;
    private final String mUrl;
    private Callback<Drawable> callback;
    private int mIconSize;
    private int mTextSize = FAVICON_TEXT_SIZE_DP;
//    private int mBackgroundColor = FAVICON_BACKGROUND_COLOR;

    private FaviconUtil(Context context, String url) {
//        mUrl = Uri.parse(url).toString();
        mUrl = url;
//        mWebsite = website;
        mContext = context;
        mIconSize = context.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        mFaviconHelper = new FaviconHelper();
    }

    public static FaviconUtil with(Context context, String url) {
        return new FaviconUtil(context, url);
    }

    public FaviconUtil setCallback(Callback<Drawable> callback) {
        this.callback = callback;
        return this;
    }

    public FaviconUtil setIconSize(int iconSize) {
        this.mIconSize = iconSize;
        return this;
    }

    public FaviconUtil setTextSize(int textSize) {
        this.mTextSize = textSize;
        return this;
    }

//    public FaviconUtil setBackgroundColor(int backgroundColor) {
//        this.mBackgroundColor = backgroundColor;
//        return this;
//    }

    public void start() {
//        if (!ChromeActivity.fromContext(mContext).didFinishNativeInitialization()) {
//            callback(null);
//            return;
//        }
        boolean result = mFaviconHelper.getLocalFaviconImageForURL(
                Profile.getLastUsedRegularProfile(),
                mUrl,
                mIconSize,
                new FaviconHelper.FaviconImageCallback() {
                    @Override
                    public void onFaviconAvailable(Bitmap image, GURL iconUrl) {
                        callback(image);
                    }
                });
        if (!result) {
            callback(null);
        }
    }

    private void callback(Bitmap bitmap) {
        if (callback != null) {
            callback.onResult(getDefaultFavicon(bitmap));
        }
    }

    private Drawable getDefaultFavicon(Bitmap bitmap) {
        Resources resources = mContext.getResources();
        if (bitmap == null) {
            // Invalid favicon, produce a generic one.
            return getDefaultFavicon();
        }
        return new CircleDrawable(mContext, bitmap);
//        return new BitmapDrawable(resources, bitmap);
    }

    public Drawable getDefaultFavicon() {
        Resources resources = mContext.getResources();
        float density = resources.getDisplayMetrics().density;
//        int faviconSizeDp = Math.round(mIconSize / density);
        int faviconSizeDp = mIconSize;
        RoundedIconGenerator faviconGenerator = new RoundedIconGenerator(resources,
                faviconSizeDp, faviconSizeDp, faviconSizeDp / 2,
                getRandomColor(), (int) (mTextSize * density));
        Bitmap bitmap = faviconGenerator.generateIconForUrl(mUrl);
        return new BitmapDrawable(resources, bitmap);
    }

    private int getRandomColor() {
        return Color.parseColor(COLORS[(int)(Math.random() * COLORS.length)]);
    }

//    private String faviconUrl() {
//        String origin = mWebsite.getAddress().getOrigin();
//        Uri uri = Uri.parse(origin);
//        if (uri.getPort() != -1) {
//            // Remove the port.
//            uri = uri.buildUpon().authority(uri.getHost()).build();
//        }
//        return uri.toString();
//    }

}

