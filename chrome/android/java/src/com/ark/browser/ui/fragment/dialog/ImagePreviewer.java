package com.ark.browser.ui.fragment.dialog;

import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.ark.browser.core.ArkWebContents;
import com.ark.browser.core.ArkWebManager;
import com.ark.browser.settings.Keys;
import com.bumptech.glide.Glide;
import com.bumptech.glide.load.engine.DiskCacheStrategy;
import com.bumptech.glide.request.RequestOptions;
import com.zpj.bus.EventLiveData;
import com.zpj.fragmentation.dialog.impl.FullScreenDialogFragment;
import com.zpj.recyclerview.EasyRecycler;

import org.chromium.chrome.R;

import java.util.List;

public class ImagePreviewer extends FullScreenDialogFragment {

    private RequestOptions options;

    private ArkWebContents mWeb;

    public static void start(Context context, int pageId) {
        ImagePreviewer previewer = new ImagePreviewer();
        Bundle bundle = new Bundle();
        bundle.putInt(Keys.KEY_ID, pageId);
        previewer.setArguments(bundle);
        previewer.show(context);
    }

    @Override
    protected int getImplLayoutId() {
        return R.layout.fragment_dialog_image_previewer;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        int mPageId = getArguments().getInt(Keys.KEY_ID, -1);
        if (mPageId < 0) {
            pop();
            return;
        }
        mWeb = ArkWebManager.get(mPageId);
        if (mWeb == null) {
            pop();
            return;
        }
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);

        if (mWeb == null) {
            return;
        }

        options = new RequestOptions()
//                .placeholder(context.getResources().getDrawable(R.mipmap.placeholder))
                .diskCacheStrategy(DiskCacheStrategy.DATA);

        EasyRecycler<String> recycler = new EasyRecycler<>(findViewById(R.id.recycler_view));
        recycler.setLayoutManager(new LinearLayoutManager(context))
                .setItemRes(R.layout.item_image)
                .onBindViewHolder((easyViewHolder, list, i, payloads) -> {
                    ImageView imageView = easyViewHolder.getImageView(R.id.iv_image);
                    Glide.with(easyViewHolder.getItemView())
                            .load(list.get(i))
                            .apply(options)
                            .into(imageView);
                })
                .setRecyclerListener(new RecyclerView.RecyclerListener() {
                    @Override
                    public void onViewRecycled(@NonNull RecyclerView.ViewHolder viewHolder) {
                        try {
                            Glide.with(context).clear(viewHolder.itemView);
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                    }
                })
                .build();
        recycler.showLoading();


        mWeb.getImagesLiveData().observe(new EventLiveData.LiveDataObserver<List<String>>() {
            @Override
            public void onChanged(@Nullable List<String> strings) {
                recycler.setItems(strings);
                recycler.showContent();
            }

            @Override
            public void onAttach() {

            }

            @Override
            public void onDetach() {

            }

            @Override
            public boolean isBindTo(Object o) {
                return true;
            }

            @Override
            public boolean isActive() {
                return true;
            }
        });

    }



//    static class PictureListAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
//        private static final String TAG = "PictureListAdapter";
//        private Context context;
//
//        public List<String> getList() {
//            return list;
//        }
//
//        private List<String> list;
//        private OnItemClickListener onItemClickListener;
//        private RequestOptions options;
//        private String url;
//        private boolean horizontal;
//
//        PictureListAdapter(Context context, List<String> list, String url, boolean horizontal) {
//            this.context = context;
//            this.list = list;
//            this.url = url;
//            options = new RequestOptions();
//            this.horizontal = horizontal;
//            options.placeholder(context.getResources().getDrawable(R.mipmap.placeholder))
//                    .diskCacheStrategy(DiskCacheStrategy.DATA);
//        }
//
//        interface OnItemClickListener {
//            void onClick(View view, int position);
//
//            void onLongClick(View view, int position);
//        }
//
//        public void setOnItemClickListener(OnItemClickListener onItemClickListener) {
//            this.onItemClickListener = onItemClickListener;
//        }
//
//        @Override
//        public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
//            return new Holder(LayoutInflater.from(context).inflate(
//                    horizontal ? R.layout.item_pic_horizontal : R.layout.item_pic, parent, false));
//        }
//
//        @Override
//        public void onViewRecycled(@NonNull RecyclerView.ViewHolder holder) {
//            super.onViewRecycled(holder);
//            if (holder instanceof Holder) {
//                Holder baseHolder = (Holder) holder;
//                if (baseHolder.getImageView() != null && context != null) {
//                    if (context instanceof Activity && ((Activity) context).isFinishing()) {
//                        return;
//                    }
//                    try {
//                        Glide.with(context).clear(baseHolder.getImageView());
//                    } catch (Exception e) {
//                        e.printStackTrace();
//                    }
//                }
//            }
//        }
//
//        @Override
//        public void onBindViewHolder(@NonNull final RecyclerView.ViewHolder viewHolder, int position) {
//            if (viewHolder instanceof Holder) {
//                Holder holder = (Holder) viewHolder;
//                GlideUtil.loadFullPicDrawable(context, holder.imgView, GlideUtil.getGlideUrl(url, list.get(position)), options);
//                holder.imgView.setOnClickListener(v -> {
//                    if (onItemClickListener != null) {
//                        if (holder.getAdapterPosition() >= 0 && holder.getAdapterPosition() < list.size()) {
//                            onItemClickListener.onClick(v, holder.getAdapterPosition());
//                        }
//                    }
//                });
//                holder.imgView.setOnLongClickListener(v -> {
//                    if (onItemClickListener != null) {
//                        if (holder.getAdapterPosition() >= 0 && holder.getAdapterPosition() < list.size()) {
//                            onItemClickListener.onLongClick(v, holder.getAdapterPosition());
//                        }
//                    }
//                    return true;
//                });
//            }
//        }
//
//        @Override
//        public int getItemCount() {
//            return list.size();
//        }
//
//        private static class Holder extends RecyclerView.ViewHolder {
//            ImageView imgView;
//
//            Holder(View itemView) {
//                super(itemView);
//                imgView = itemView.findViewById(R.id.item_reult_img);
//            }
//
//            public ImageView getImageView() {
//                return imgView;
//            }
//        }
//    }

}
