package com.ark.browser.tab;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.LruCache;
import android.util.SparseArray;
import android.widget.ImageView;

import androidx.annotation.NonNull;

import com.ark.browser.core.ArkWebContents;
import com.ark.browser.core.ArkWebManager;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.ui.widget.FitWidthImageView;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;
import com.zpj.utils.FileUtils;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.RenderWidgetHostView;

import java.io.File;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * 管理web快照
 * @author Z-P-J
 */
public class PageSnapshotManager {

    private static final String THUMBNAIL_DIRECTORY_NAME = "thumbnail";

    private final SparseArray<SnapshotTask> snapshotTasks = new SparseArray<>();
    private final LruCache<Integer, Bitmap> mBitmapCache;

    private static final class Holder {
        private static final PageSnapshotManager MANAGER = new PageSnapshotManager();
    }

    private static final class PathHolder {
        private static final String path = ContextUtils.getApplicationContext().getDir(
                THUMBNAIL_DIRECTORY_NAME, Context.MODE_PRIVATE).getPath();
    }

    private PageSnapshotManager() {
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

    public static PageSnapshotManager getInstance() {
        return Holder.MANAGER;
    }

    public void copySnapshot(int oldId, int newId) {
        ThreadPool.executeIO(() -> {
            synchronized (snapshotTasks) {
                File file = new File(PathHolder.path, oldId + ".thumbnail");
                if (file.exists()) {
                    File newFile = new File(PathHolder.path, newId + ".thumbnail");
                    FileUtils.copyFileFast(file, newFile);
                }
            }
        });
    }

    public void cacheCurrentPage() {
        ITab tab = TabListManager.getInstance().getCurrentTab();
        if (tab == null) {
            return;
        }
        cachePage(tab.getCurrentPageInfo());
    }

    public void cachePage(PageInfo pageInfo) {
        if (pageInfo == null) {
            return;
        }
        ArkLogger.d("TabThumbnailManager", "cacheTab page=" + pageInfo.getId());
        synchronized (snapshotTasks) {
            int pageId = pageInfo.getId();
            SnapshotTask task = snapshotTasks.get(pageId);
            if (task == null) {
                task = new SnapshotTask(pageId, null);
                snapshotTasks.put(pageId, task);
            } else {
                task.cancel();
            }
            task.start();
        }
    }

    public void removeSnapshot(int pageId) {
        ThreadPool.executeIO(() -> {
            synchronized (mBitmapCache) {
                mBitmapCache.remove(pageId);
            }
            File file = new File(PathHolder.path, pageId + ".thumbnail");
            if (file.exists()) {
                file.delete();
            }
        });
    }

    public void loadSnapshot(int pageId, @NonNull Callback<Bitmap> callback) {
        if (pageId != Tab.INVALID_PAGE_ID) {
            synchronized (snapshotTasks) {
                SnapshotTask task = snapshotTasks.get(pageId);
                if (task == null) {
                    synchronized (mBitmapCache) {
                        Bitmap bitmap = mBitmapCache.get(pageId);
                        if (bitmap == null) {
                            File file = new File(PathHolder.path, pageId + ".thumbnail");
                            if (file.exists()) {
                                ThreadPool.executeIO(() -> {
                                    Bitmap bitmap1 = BitmapFactory.decodeFile(file.getPath());
                                    ThreadPool.runOnUIThread(() -> callback.onResult(bitmap1));
                                });
                                return;
                            }

                            task = new SnapshotTask(pageId, callback);
                            snapshotTasks.put(pageId, task);
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

    public void loadSnapshot(ImageView ivThumbnail, int pageId) {
        loadSnapshot(pageId, value -> {
            if (value == null) {
//                    ivThumbnail.setImageResource(R.drawable.qianxun_home_wallpaper);
                ivThumbnail.setImageBitmap(null);
            } else {
                ivThumbnail.setImageBitmap(value);
            }
        });
    }

//    public void loadSnapshot(ImageView ivThumbnail, Tab tab) {
//        loadSnapshot(ivThumbnail, tab == null ? Tab.INVALID_PAGE_ID : tab.getId());
//    }

    public void loadSnapshot(ImageView ivThumbnail, PageInfo pageInfo) {
        loadSnapshot(ivThumbnail, pageInfo.getId());
    }

    public void loadSnapshot(FitWidthImageView ivThumbnail, PageInfo pageInfo) {
        loadSnapshot(ivThumbnail, pageInfo.getId());
    }

    public void loadSnapshot(FitWidthImageView ivThumbnail, int pageId) {
        loadSnapshot(pageId, value -> {
            if (value == null) {
//                    ivThumbnail.setImageResource(R.drawable.qianxun_home_wallpaper);
                ivThumbnail.setImageBitmap(null);
            } else {
                ivThumbnail.setImageBitmap(value);
            }
        });
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

            ArkWebContents arkWeb = ArkWebManager.get(mPageId);
            if (arkWeb != null && !arkWeb.isDestroyed()) {
                RenderWidgetHostView renderWidgetHostView = arkWeb.getWebContents().getRenderWidgetHostView();
                if (renderWidgetHostView == null) {
                    onFinished(null);
                } else {
                    File target = new File(PathHolder.path, mPageId + ".thumbnail");
                    renderWidgetHostView.saveContentBitmapToDiskAsync(0, 0, target.getAbsolutePath(), new Callback<String>() {
                        @Override
                        public void onResult(String result) {
                            ArkLogger.e(SnapshotTask.class, "onResult result=" + result);
                            ThreadPool.executeIO(() -> {
                                File file = new File(result);
                                if (file.exists()) {
                                    try (FileInputStream fis = new FileInputStream(file)) {
                                        Bitmap bitmap = BitmapFactory.decodeStream(fis);
                                        ThreadPool.runOnUIThread(() -> onFinished(bitmap));
                                        synchronized (getInstance().mBitmapCache) {
                                            getInstance().mBitmapCache.put(mPageId, bitmap);
                                        }
                                        return;
                                    } catch (Exception e) {
                                        ArkLogger.e(SnapshotTask.class, "decodeBitmap failed! ", e);
                                    }
                                }
                                ThreadPool.runOnUIThread(() -> onFinished(null));
                            });
                        }
                    });
                }
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
