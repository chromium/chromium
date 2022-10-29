package com.ark.browser.tab;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.LruCache;
import android.util.SparseArray;
import android.widget.ImageView;

import androidx.annotation.NonNull;

import com.ark.browser.tab.core.IPage;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.tab.Tab;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * 管理tab快照
 * @author Z-P-J
 */
public class TabSnapshotManager {

    private static final String THUMBNAIL_DIRECTORY_NAME = "thumbnail";

    private final SparseArray<SnapshotTask> snapshotTasks = new SparseArray<>();
    private final LruCache<Integer, Bitmap> mBitmapCache;

//    private final String path;

    private static final class Holder {
        private static final TabSnapshotManager MANAGER = new TabSnapshotManager();
    }

    private static final class PathHolder {
        private static final String path = ContextUtils.getApplicationContext().getDir(
                THUMBNAIL_DIRECTORY_NAME, Context.MODE_PRIVATE).getPath();
    }

    private TabSnapshotManager() {
        long maxMemory = Runtime.getRuntime().maxMemory() / 6;
        // 89478485
        ArkLogger.d("TabThumbnailManager", "maxMemory=" + maxMemory);
        mBitmapCache = new LruCache<Integer, Bitmap>((int) maxMemory) {
            @Override
            protected int sizeOf(Integer key, Bitmap value) {
                return value.getByteCount();
            }
        };
    }

    public static TabSnapshotManager getInstance() {
        return Holder.MANAGER;
    }

    public void cacheCurrentTab() {
        IPage page = TabListManager.getInstance().getCurrentPage();
        Tab currentTab = page == null ? null : page.getNativePage();
        cacheTab(currentTab);
    }

    public void cacheTab(Tab tab) {
        if (tab == null) {
            return;
        }
        ArkLogger.d("TabThumbnailManager", "cacheTab tab=" + tab.getId());
        synchronized (snapshotTasks) {
            int tabId = tab.getId();
            SnapshotTask task = snapshotTasks.get(tabId);
            if (task == null) {
                task = new SnapshotTask(tabId, null);
                snapshotTasks.put(tabId, task);
            } else {
                task.cancel();
            }
            task.start();
        }
    }

    public void removeSnapshot(int pageId) {
        ThreadPool.executeIO(() -> {
            File file = new File(PathHolder.path, pageId + ".thumbnail");
            if (file.exists()) {
                file.delete();
            }
        });
    }

    public void loadTabSnapshot(int tabId, @NonNull Callback<Bitmap> callback) {
        if (tabId != Tab.INVALID_PAGE_ID) {
            synchronized (snapshotTasks) {
                SnapshotTask task = snapshotTasks.get(tabId);
                if (task == null) {
                    synchronized (mBitmapCache) {
                        Bitmap bitmap = mBitmapCache.get(tabId);
                        if (bitmap == null) {
                            File file = new File(PathHolder.path, tabId + ".thumbnail");
                            if (file.exists()) {
                                ThreadPool.executeIO(() -> {
                                    Bitmap bitmap1 = BitmapFactory.decodeFile(file.getPath());
                                    ThreadPool.post(() -> callback.onResult(bitmap1));
                                });
                                return;
                            }

                            task = new SnapshotTask(tabId, callback);
                            snapshotTasks.put(tabId, task);
                            task.start();
                        } else {
                            callback.onResult(bitmap);
                        }
                    }
                } else {
                    task.cancel();
                    task.addCallback(callback);
                    task.start();
                }
            }
        } else {
            callback.onResult(null);
        }
    }

    public void loadTabSnapshot(ImageView ivThumbnail, int tabId) {
        loadTabSnapshot(tabId, value -> {
            if (value == null) {
//                    ivThumbnail.setImageResource(R.drawable.qianxun_home_wallpaper);
                ivThumbnail.setImageBitmap(null);
            } else {
                ivThumbnail.setImageBitmap(value);
            }
        });
    }

    public void loadTabSnapshot(ImageView ivThumbnail, Tab tab) {
        loadTabSnapshot(ivThumbnail, tab == null ? Tab.INVALID_PAGE_ID : tab.getId());
    }

    public void loadTabSnapshot(ImageView ivThumbnail, PageInfo pageInfo) {
        loadTabSnapshot(ivThumbnail, pageInfo.getPageId());
    }

    private static class SnapshotTask {

        private final List<Callback<Bitmap>> mCallbacks = new ArrayList<>();
        private final int mPageId;

        private final AtomicBoolean mStart = new AtomicBoolean();

        public SnapshotTask(int pageId, Callback<Bitmap> callback) {
            this.mPageId = pageId;
            addCallback(callback);
        }

        public void addCallback(Callback<Bitmap> callback) {
            synchronized (mCallbacks) {
                if (callback != null) {
                    mCallbacks.add(callback);
                }
            }
        }

        public void start() {
            mStart.set(true);
            Tab tab = PageCacheManager.getInstance().findPage(mPageId);
            if (tab != null && tab.getWebContents() != null) {
//                tab.getWebContents().getContentBitmapAsync(0, 0, (bitmap, response) -> {
//                    if (mStart.get()) {
//                        Log.d("TabThumbnailManager.Task", "onFinishGetBitmap size=" + getInstance().mBitmapCache.size() + " maxSize=" + getInstance().mBitmapCache.maxSize());
//                        Log.d("TabThumbnailManager.Task", "onFinishGetBitmap bitmap=" + bitmap + " tab=" + mPageId);
////                            int themeColor;
//                        if (bitmap != null) {
//
////                                themeColor = bitmap.getPixel(1, 1);
//
//                            synchronized (getInstance().mBitmapCache) {
//                                getInstance().mBitmapCache.put(mPageId, bitmap);
//                            }
//                            ThreadPool.executeIO(() -> {
//                                long start = System.currentTimeMillis();
//                                File file = new File(getInstance().path, mPageId + ".thumbnail");
//
////                                Matrix matrix = new Matrix();
////                                matrix.postScale(0.8f, 0.8f);
////                                Bitmap newbm = Bitmap.createBitmap(bitmap, 0, 0,
////                                        bitmap.getWidth(), bitmap.getHeight(),
////                                        matrix, true);
//
//                                try {
//                                    bitmap.compress(Bitmap.CompressFormat.JPEG, 80,
//                                            new BufferedOutputStream(new FileOutputStream(file)));
////                                    synchronized (getInstance().mBitmapCache) {
////                                        getInstance().mBitmapCache.put(mPageId, newbm);
////                                    }
//                                } catch (FileNotFoundException e) {
//                                    e.printStackTrace();
//                                }
//                                Log.d("TabThumbnailManager.Task",
//                                        "onFinishGetBitmap saveBitmap deltaTime="
//                                                + (System.currentTimeMillis() - start));
//                            });
//                        } else {
////                                themeColor = mTab.getThemeColor() == 0 ? Color.WHITE : mTab.getThemeColor();
//                        }
////                            mTab.setThemeColor(themeColor);
////                            EventBus.postWebColorChangeEvent(mTab, themeColor);
//                        onFinished(bitmap);
//                    }
//                });
                onFinished(null);
            } else {
                onFinished(null);
            }
        }

        public boolean isRunning() {
            return mStart.get();
        }

        public void cancel() {
            mStart.set(false);
        }

        public void onFinished(Bitmap bitmap) {
            synchronized (getInstance().snapshotTasks) {
                for (Callback<Bitmap> callback : mCallbacks) {
                    callback.onResult(bitmap);
                }
                mCallbacks.clear();
                getInstance().snapshotTasks.remove(mPageId);
            }
            mStart.set(false);
        }


    }


}
