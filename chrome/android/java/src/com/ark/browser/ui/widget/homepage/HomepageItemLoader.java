package com.ark.browser.ui.widget.homepage;

import static com.ark.browser.utils.ShadowGenerator.BLUR_FACTOR;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;

import com.android.launcher3.DeviceProfile;
import com.android.launcher3.ItemInfo;
import com.android.launcher3.LauncherLayout;
import com.android.launcher3.LauncherManager;
import com.android.launcher3.database.SQLite;
import com.android.launcher3.database.table.FavoriteItemTable;
import com.android.launcher3.graphics.ColorExtractor;
import com.android.launcher3.model.FavoriteItem;
import com.ark.browser.tab.TabListFaviconProvider;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ShadowGenerator;
import com.zpj.utils.Callback;
import com.zpj.utils.ColorUtils;
import com.zpj.utils.ContextUtils;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.url.GURL;

public class HomepageItemLoader implements LauncherLayout.ItemLoader {

    private final Context mContext;
    private TabListFaviconProvider mTabListFaviconProvider;
    private final Paint mFaviconBackgroundPaint;

    public HomepageItemLoader(Context context) {
        mContext = context;

        mFaviconBackgroundPaint = new Paint();
        mFaviconBackgroundPaint.setAntiAlias(true);
        mFaviconBackgroundPaint.setColor(Color.WHITE);
        mFaviconBackgroundPaint.setStyle(Paint.Style.FILL);
        Resources resources = context.getResources();
        mFaviconBackgroundPaint.setShadowLayer(
                resources.getDimension(org.chromium.chrome.R.dimen.tab_grid_thumbnail_favicon_background_radius), 0,
                resources.getDimension(org.chromium.chrome.R.dimen.tab_grid_thumbnail_favicon_background_down_shift),
                resources.getColor(org.chromium.chrome.R.color.modern_grey_800_alpha_38));
    }

    @Override
    public void onFirstRun() {
        SQLite.with(FavoriteItemTable.class).delete();
        for (ItemInfo info : HomepageUtils.initHomeNav()) {
            FavoriteItem.from(info).insert();
        }
    }

    @Override
    public View onCreateSearchBar(ViewGroup root) {
        Context context = root.getContext();
        return new HomepageSearchBar(context);
    }

    @Override
    public void loadIcon(ItemInfo itemInfo, Callback<Bitmap> callback) {
        Resources resources = mContext.getResources();
        int resId = org.chromium.chrome.R.mipmap.app_icon;
        if (HomepageUtils.isDeepLink(itemInfo.url)) {
            switch (itemInfo.url) {
                case HomepageUtils.DEEPLINK_MANAGER:
                    resId = org.chromium.chrome.R.drawable.icon_browser_manager;
                    break;
                case HomepageUtils.DEEPLINK_COLLECTIONS:
                    resId = org.chromium.chrome.R.drawable.icon_collections;
                    break;
                case HomepageUtils.DEEPLINK_BROWSER:
                    resId = org.chromium.chrome.R.drawable.icon_browser;
                    break;
                case HomepageUtils.DEEPLINK_DOWNLOADS:
                    resId = org.chromium.chrome.R.drawable.icon_download_manager;
                    break;
                case HomepageUtils.DEEPLINK_SETTINGS:
                    resId = org.chromium.chrome.R.drawable.icon_settings;
                    break;
            }
            Bitmap icon = BitmapFactory.decodeResource(resources, resId);
            int color = ColorExtractor.findDominantColorByHue(icon);
            color = ColorUtils.getDarkenedColor(color, 1.2f);
            callback.onCallback(decorateBitmap(BitmapFactory.decodeResource(resources, resId), color));
        } else {
            if (ProfileManager.isInitialized()) {
                loadItemIcon(itemInfo, callback);
            } else {
                ProfileManager.addObserver(new ProfileManager.Observer() {
                    @Override
                    public void onProfileAdded(Profile profile) {
                        ProfileManager.removeObserver(this);
                        loadItemIcon(itemInfo, callback);
                    }

                    @Override
                    public void onProfileDestroyed(Profile profile) {
                        ProfileManager.removeObserver(this);
                    }
                });
            }
        }
    }

    private void loadItemIcon(ItemInfo itemInfo, Callback<Bitmap> callback) {
        if (mTabListFaviconProvider == null) {
            mTabListFaviconProvider = new TabListFaviconProvider(mContext, false);
            mTabListFaviconProvider.initWithNative(Profile.getLastUsedRegularProfile());
        }
        mTabListFaviconProvider.getFaviconForUrlAsync(
                new GURL(itemInfo.url), false,
                favicon -> {
                    DeviceProfile grid = LauncherManager.getDeviceProfile();
                    int size = grid.iconSizePx;
                    float radius = size * (0.28f - BLUR_FACTOR);

                    Bitmap mMultiThumbnailBitmap = Bitmap.createBitmap(
                            size, size, Bitmap.Config.ARGB_8888);
                    Canvas mCanvas = new Canvas(mMultiThumbnailBitmap);
                    mCanvas.drawColor(Color.TRANSPARENT);

//                        int color = ColorUtils.getDarkenedColor(
//                                favicon.getDominantColor(), 1.2f);
                    mFaviconBackgroundPaint.setColor(Color.WHITE);
                    float padding = BLUR_FACTOR * size;
                    mCanvas.drawRoundRect(padding, padding, size - padding,
                            size - padding, radius, radius, mFaviconBackgroundPaint);
                    int start = (int) (size / 4);
                    int end = (int) (size - start);
                    Drawable drawable = favicon.getDefaultDrawable();
                    drawable.setBounds(start, start, end, end);
                    drawable.draw(mCanvas);
                    callback.onCallback(mMultiThumbnailBitmap);
                });
    }

    public Bitmap decorateBitmap(Bitmap icon, int bgColor) {
        long start = System.currentTimeMillis();
        DeviceProfile grid = LauncherManager.getDeviceProfile();
        int mIconSize = grid.iconSizePx;

        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setStyle(Paint.Style.FILL_AND_STROKE);
        paint.setStrokeJoin(Paint.Join.ROUND);
        paint.setStrokeCap(Paint.Cap.ROUND);

        Bitmap bitmap = Bitmap.createBitmap(mIconSize, mIconSize, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas();
        canvas.setBitmap(bitmap);
        paint.setColor(bgColor);
        float r = mIconSize * (0.28f - BLUR_FACTOR);
        float padding = BLUR_FACTOR * mIconSize;
        canvas.drawRoundRect(padding, padding, mIconSize - padding, mIconSize - padding, r, r, paint);
        Matrix matrix = new Matrix();
        float scale = 0.8f;
        matrix.setScale(scale, scale);
        matrix.postTranslate((mIconSize - scale * icon.getWidth()) / 2f, (mIconSize - scale * icon.getWidth()) / 2f);
        canvas.drawBitmap(icon, matrix, paint);

        Bitmap newBitmap = Bitmap.createBitmap(mIconSize, mIconSize, Bitmap.Config.ARGB_8888);
        canvas.setBitmap(newBitmap);
        new ShadowGenerator(ContextUtils.getApplicationContext()).recreateIcon(bitmap, canvas);
        bitmap.recycle();

        ArkLogger.d(this, "deltaTime=" + (System.currentTimeMillis() - start));

        return newBitmap;
    }

}
