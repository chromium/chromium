package com.ark.browser.ui.fragment.wallpaper;

import android.graphics.Bitmap;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.ark.browser.utils.ThreadPool;
import com.bumptech.glide.Glide;
import com.bumptech.glide.request.target.Target;
import com.zpj.fragmentation.dialog.imageviewer.ImageViewerDialogFragment;
import com.zpj.toast.ZToast;
import com.zpj.utils.CipherUtils;
import com.zpj.utils.ScreenUtils;

import org.chromium.chrome.R;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

public class WallpaperPreviewer extends ImageViewerDialogFragment<String> {

    private View mSearchBar;

    @Override
    public void onSupportVisible() {
        super.onSupportVisible();
        lightStatusBar();
    }

    @Override
    protected int getCustomLayoutId() {
        return R.layout.fragment_wallpaper_previewer;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);

        TextView sureBtn = view.findViewById(R.id.btn_sure);
        mSearchBar = findViewById(R.id.search_bar);

        int cellHeight = ScreenUtils.getScreenHeight() / 8;

        ViewGroup.LayoutParams params = mSearchBar.getLayoutParams();
        if (params != null) {
            params.height = 3 * cellHeight;
        } else {
            params = new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 3 * cellHeight);
        }
        mSearchBar.setLayoutParams(params);

        sureBtn.setOnClickListener(v -> setWallpaper(pager.getCurrentItem()));
        findViewById(R.id.btn_show_searchbar).setOnClickListener(v -> {
            if (mSearchBar.getVisibility() == View.VISIBLE) {
                mSearchBar.setVisibility(View.INVISIBLE);
            } else {
                mSearchBar.setVisibility(View.VISIBLE);
            }
        });
    }

    @Override
    public void onShowAnimationUpdate(float percent) {
        super.onShowAnimationUpdate(percent);
    }

    @Override
    public void onDismissAnimationUpdate(float percent) {
        super.onDismissAnimationUpdate(percent);
    }

    private void setWallpaper(int pos) {
        String url = urls.get(pos);
        ThreadPool.execute(() -> {
            File file = getImageFile(url);
            try {

                if (!file.exists()) {
                    Bitmap bitmap = Glide.with(context).asBitmap().load(url)
                            .into(Target.SIZE_ORIGINAL, Target.SIZE_ORIGINAL)
                            .get();
                    saveImage(bitmap, file);
                }


                String path = file.getAbsolutePath();
                WallpaperManager.setWallpaperPath(path);
                post(() -> {
                    ZToast.success("主页壁纸设置成功！");
                });
            } catch (Exception e) {
                e.printStackTrace();
                file.delete();
                post(() -> ZToast.error("设置壁纸失败！" + e.getMessage()));
            }
        });
    }

    private void saveImage(Bitmap bmp, File file) {
        if (getContext() == null) {
            return;
        }
        FileOutputStream fos = null;
        try {
            fos = new FileOutputStream(file);
            bmp.compress(Bitmap.CompressFormat.PNG, 100, fos);
            fos.flush();
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            try {
                if (fos != null) {
                    fos.close();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    private File getImageFile(String url) {
        File file = getContext().getExternalCacheDir();//注意小米手机必须这样获得public绝对路径
        File appDir = new File(file ,"Wallpaper");
        if (!appDir.exists()) {
            appDir.mkdirs();
        }
        return new File(appDir, CipherUtils.md5(url));
    }

}

