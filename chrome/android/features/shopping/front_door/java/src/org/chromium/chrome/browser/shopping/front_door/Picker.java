package org.chromium.chrome.browser.shopping.front_door;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.view.View;
import android.widget.CheckBox;
import android.widget.ImageView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.GridLayoutManager.SpanSizeLookup;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ViewBuilder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

public class Picker {
    @IntDef({ListType.GRID, ListType.LINEAR_HORIZONTAL, ListType.LINEAR_VERTICAL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ListType {
        int GRID = 0;
        int LINEAR_HORIZONTAL = 1;
        int LINEAR_VERTICAL = 2;
    }

    private final RecyclerView mRecyclerView;
    private final SimpleRecyclerViewAdapter mAdapter;
    private final Context mContext;
    private final Map<Integer, Integer> mSpanSizeMap = new HashMap<>();
    private final boolean mIsCategoryPicker;

    public Picker(Context context, MVCListAdapter.ModelList model, @ListType int listType,
            boolean isCategoryPicker) {
        mContext = context;
        mRecyclerView = new RecyclerView(context);
        mAdapter = new SimpleRecyclerViewAdapter(model);
        mIsCategoryPicker = isCategoryPicker;

        switch (listType) {
            case ListType.GRID:
                GridLayoutManager layoutManager =
                        new GridLayoutManager(context, isCategoryPicker ? 3 : 2);
                layoutManager.setSpanSizeLookup(new SpanSizeLookup() {
                    @Override
                    public int getSpanSize(int i) {
                        int itemViewType = mAdapter.getItemViewType(i);

                        if (!mSpanSizeMap.containsKey(itemViewType)) return 1;

                        int regiesteredSpanSize = mSpanSizeMap.get(itemViewType);
                        int layoutManagerSpanSize = layoutManager.getSpanCount();
                        return Math.min(regiesteredSpanSize, layoutManagerSpanSize);
                    }
                });
                mRecyclerView.setLayoutManager(layoutManager);

                int topPadding =
                        (int) context.getResources().getDimension(R.dimen.pick_top_padding);
                int horizontalPadding = (int) context.getResources().getDimension(
                        R.dimen.picker_horizontal_padding);
                mRecyclerView.setPadding(horizontalPadding, topPadding, horizontalPadding, 0);

                break;
            case ListType.LINEAR_HORIZONTAL:
                mRecyclerView.setLayoutManager(
                        new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false));
                // mRecyclerView.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT,
                //     context.getResources().getDimensionPixelSize(R.dimen.resume_recycler_view_height)));
                break;
            case ListType.LINEAR_VERTICAL:
                mRecyclerView.setLayoutManager(
                        new LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false));
                break;
        }

        mRecyclerView.setAdapter(mAdapter);

        RecyclerView.RecyclerListener recyclerListener = (holder) -> {
            Log.e("Meil", "picker on recycle");
            View view = holder.itemView;

            if (isCategoryPicker) {
            } else {
                ImageView favicon = (ImageView) view.findViewById(R.id.favicon);
                clearResource(favicon);

                ImageView startImage = (ImageView) view.findViewById(R.id.start_image);
                clearResource(startImage);

                ImageView endTopImage = (ImageView) view.findViewById(R.id.end_top_image);
                clearResource(endTopImage);

                ImageView endBottomImage = (ImageView) view.findViewById(R.id.end_bottom_image);
                clearResource(endBottomImage);
            }
        };
        mRecyclerView.setRecyclerListener(recyclerListener);
    }

    private void clearResource(ImageView imageView) {
        if (imageView == null) {
            Log.e("Meil_pickerRecyclerListen", "image view is null");
            return;
        }
        imageView.setImageResource(org.chromium.chrome.browser.shopping.front_door.R.color
                                           .thumbnail_placeholder_on_primary_bg);
    }

    public View getView() {
        return mRecyclerView;
    }

    public void destroy() {
        mAdapter.destroy();
    }

    public <T extends View> void registerItemView(int typeId, ViewBuilder<T> builder,
            ViewBinder<PropertyModel, T, PropertyKey> binder, int spanSize) {
        registerItemView(typeId, builder, binder);

        mSpanSizeMap.put(typeId, spanSize);
    }

    public <T extends View> void registerItemView(
            int typeId, ViewBuilder<T> builder, ViewBinder<PropertyModel, T, PropertyKey> binder) {
        PickItemBinder<T> pickItemBinder = new PickItemBinder<>(mContext, binder);
        mAdapter.registerType(typeId, builder, pickItemBinder::bind);
    }

    // All caller need to has this in the item model. If IS_PICKABLE is true, it must have the
    // PICKER_EFFECT_CALLBACK.
    public static class PickerItemPropertyModel {
        public static final PropertyModel.ReadableBooleanPropertyKey IS_PICKABLE =
                new ReadableBooleanPropertyKey();
        public static final PropertyModel.WritableBooleanPropertyKey IS_PICKED =
                new PropertyModel.WritableBooleanPropertyKey();
        public static final PropertyModel
                .WritableObjectPropertyKey<Callback<View>> PICKER_EFFECT_CALLBACK =
                new WritableObjectPropertyKey<>();
    }

    private static class PickItemBinder<T extends View> {
        final ViewBinder<PropertyModel, T, PropertyKey> mBinder;
        final int mSelectedViewPadding;
        final Drawable mSelectedViewForgroundDrawable;

        PickItemBinder(Context context, ViewBinder<PropertyModel, T, PropertyKey> binder) {
            mBinder = binder;
            mSelectedViewPadding =
                    (int) context.getResources().getDimension(R.dimen.picker_selected_padding);
            mSelectedViewForgroundDrawable =
                    AppCompatResources.getDrawable(context, R.drawable.picked_indicator);
        }

        void bind(PropertyModel model, T view, @Nullable PropertyKey propertyKey) {
            if (model.get(PickerItemPropertyModel.IS_PICKABLE)) {
                if (propertyKey == PickerItemPropertyModel.IS_PICKED) {
                    updateItemView(view, model.get(PickerItemPropertyModel.IS_PICKED));
                }
            }

            mBinder.bind(model, view, propertyKey);
        }

        void updateItemView(View view, boolean isPick) {
            if (isPick) {
                picked(view);
            } else {
                notPicked(view);
            }
        }

        void picked(View view) {
            boolean isZoom = view.findViewById(R.id.item) != null;

            if (isZoom) {
                view.findViewById(R.id.item).setPadding(mSelectedViewPadding, mSelectedViewPadding,
                        mSelectedViewPadding, mSelectedViewPadding);
                if (VERSION.SDK_INT >= VERSION_CODES.M) {
                    view.findViewById(R.id.item).setForeground(mSelectedViewForgroundDrawable);
                }
            } else {
                ((CheckBox) view.findViewById(R.id.check_box)).setChecked(true);
            }
        }

        void notPicked(View view) {
            boolean isZoom = view.findViewById(R.id.item) != null;

            if (isZoom) {
                view.findViewById(R.id.item).setPadding(0, 0, 0, 0);
                if (VERSION.SDK_INT >= VERSION_CODES.M) {
                    view.findViewById(R.id.item).setForeground(null);
                }
            } else {
                ((CheckBox) view.findViewById(R.id.check_box)).setChecked(false);
            }
        }
    }
}
