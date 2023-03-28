package com.ark.browser.ui.fragment.wallpaper;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.utils.HttpUtils;
import com.bumptech.glide.Glide;
import com.bumptech.glide.request.RequestOptions;
import com.bumptech.glide.request.target.Target;
import com.zpj.fragmentation.SupportActivity;
import com.zpj.recyclerview.EasyRecycler;
import com.zpj.utils.ContextUtils;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

public class WallpaperSelectFragment extends BaseSwipeBackFragment {

    private static final String TAG = "WallpaperSelectFragment";

    private EasyRecycler<WallpaperBean> mRecycler;

    private final RequestOptions options = new RequestOptions()
            .centerCrop()
            .override(Target.SIZE_ORIGINAL);

    private int currentPage;

    public static void start(Context context) {
        WallpaperSelectFragment fragment = new WallpaperSelectFragment();
        Activity activity = ContextUtils.getActivity(context);
        if (activity instanceof SupportActivity) {
            ((SupportActivity)activity).start(fragment);
        } else {
            if (!(activity instanceof FragmentActivity)) {
                throw new RuntimeException("the context is not a FragmentActivity object!");
            }

            FragmentManager manager = ((FragmentActivity)activity).getSupportFragmentManager();
            FragmentTransaction ft = manager.beginTransaction();
            ft.add(fragment, "tag");
            ft.commit();
        }
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_wallpaper;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);

        mRecycler = new EasyRecycler<>(findViewById(R.id.recycler_view));
        mRecycler.setItemRes(R.layout.item_wallpaper)
                .setLayoutManager(new GridLayoutManager(context, 3))
                .setHeaderView(null)
                .onCreateViewHolder((parent, layoutRes, viewType) -> {
                    View view1 = LayoutInflater.from(parent.getContext()).inflate(layoutRes, parent, false);
                    GridLayoutManager.LayoutParams lp = (GridLayoutManager.LayoutParams) view1.getLayoutParams();

                    lp.height = (int) (parent.getMeasuredHeight() / 3f);
                    view1.setLayoutParams(lp);
                    return view1;
                })
                .onBindViewHolder((holder, list, position, payload) -> {
                    ImageView imageView = holder.getView(R.id.img_preview);
                    Glide.with(context)
                            .load(list.get(position).previewUrl)
                            .apply(options)
                            .into(imageView);
                    holder.setOnItemClickListener(v -> {
                        List<String> urls = new ArrayList<>();
                        for (WallpaperBean wallpaper : mRecycler.getItems()) {
                            urls.add(wallpaper.url);
                        }
                        new WallpaperPreviewer()
                                // .setScaleType(ImageViewerDialogFragment.SCALE_TYPE_CENTER_CROP)
                                .setImageUrls(urls)
                                .setSrcView(imageView, position)
                                .setSrcViewUpdateListener((popup, pos) -> {
                                    RecyclerView recyclerView = mRecycler.getRecyclerView();
                                    int layoutPos = recyclerView.indexOfChild(holder.getItemView());
                                    View child = recyclerView.getChildAt(layoutPos + pos - position);

                                    ImageView image;
                                    if (child != null) {
                                        image = child.findViewById(R.id.img_preview);
                                    } else {
                                        image = imageView;
                                    }
                                    popup.updateSrcView(image);
                                })
                                .show(context);
                    });
                })
                .onLoadMore((currentPage) -> {
                    WallpaperSelectFragment.this.currentPage = currentPage;
                    Log.d(TAG, "currentPage=" + currentPage);


                    HttpUtils.get("https://wallspic.com/cn/album/popular/for_mobile", new HttpUtils.Callback() {
                        @Override
                        public void onFailed(Exception e) {
                            mRecycler.showErrorView(e.getMessage());
                        }

                        @Override
                        public void onSuccess(String body) throws Exception {
                            Log.d(TAG, "body=" + body);

                            String html = body;
                            String str = "window.mainAdaptiveGallery = ";
                            html = html.substring(html.indexOf(str) + str.length());
                            str = "window.mainGalleryTarget = ";
                            html = html.substring(0, html.indexOf(str)).trim();

                            Log.d(TAG, "html=" + html);

                            JSONObject json = new JSONObject(html);

                            JSONArray array = json.getJSONArray("list");
                            for (int i = 0; i < array.length(); i++) {
                                Object item = array.get(i);
                                if (item instanceof JSONObject) {
                                    JSONObject original = ((JSONObject) item).getJSONObject("original");

                                    String contentUrl = original.getString("link");
                                    int width = original.getInt("width");
                                    int height = original.getInt("height");


                                    String thumbnailUrl = ((JSONObject) item).getJSONObject("thumbnail").getString("link");
                                    WallpaperBean bean = new WallpaperBean();
                                    String ext = thumbnailUrl.substring(thumbnailUrl.lastIndexOf('.') + 1);
                                    String name = contentUrl.substring(contentUrl.lastIndexOf('/') + 1);
                                    bean.previewUrl = thumbnailUrl;


                                    Log.e(TAG, "ext=" + ext + " name=" + name);

                                    String link = thumbnailUrl.replaceFirst("previews", "crops");
                                    link = link.substring(0, link.lastIndexOf('/')) + String.format("/%s-%sx%s.%s", name, width, height, ext);

                                    Log.e(TAG, "link=" + link);

                                    bean.url = link;
                                    mRecycler.addItem(bean);
                                }
                            }

                            if (mRecycler.isEmpty()) {
                                mRecycler.showEmpty();
                            } else {
                                mRecycler.showContent();
                            }
                        }
                    });
                    return true;
                })
                .build();
    }

    private static class WallpaperBean {

        private String url;

        private String previewUrl;

    }

//    public class ImageLoader implements IImageLoader<WallpaperBean> {
//
//        @Override
//        public void loadImage(int position, @NonNull WallpaperBean uri, @NonNull ImageView imageView) {
//            Glide.with(imageView).load(uri.previewUrl).apply(new RequestOptions().override(Target.SIZE_ORIGINAL)).into(imageView);
//        }
//
//        @Override
//        public File getImageFile(@NonNull Context context, @NonNull WallpaperBean uri) {
//            try {
//                return Glide.with(context).downloadOnly().load(uri.previewUrl).submit().get();
//            } catch (Exception e) {
//                e.printStackTrace();
//            }
//            return null;
//        }
//    }

}

