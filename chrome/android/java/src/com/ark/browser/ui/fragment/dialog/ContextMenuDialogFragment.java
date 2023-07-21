package com.ark.browser.ui.fragment.dialog;

import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.MENU_ID;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemProperties.TEXT;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.BUTTON_CONTENT_DESC;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.BUTTON_IMAGE;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.Shader;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.cardview.widget.CardView;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.ark.browser.utils.FaviconUtil;
import com.zpj.fragmentation.dialog.DialogAnimator;
import com.zpj.fragmentation.dialog.utils.DialogThemeUtils;
import com.zpj.recyclerview.EasyRecycler;
import com.zpj.recyclerview.EasyViewHolder;
import com.zpj.toast.ZToast;
import com.zpj.utils.ScreenUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator;
import org.chromium.chrome.browser.contextmenu.ContextMenuHeaderProperties;
import org.chromium.chrome.browser.contextmenu.ContextMenuNativeDelegate;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

public class ContextMenuDialogFragment extends RecyclerAttachDialogFragment<MVCListAdapter.ListItem>
        implements RecyclerAttachDialogFragment.Adapter<MVCListAdapter.ListItem> {

    private EasyRecycler<MVCListAdapter.ListItem> mRecycler;
    private ContextMenuParams mParams;
    private ContextMenuNativeDelegate mDelegate;

    private Callback<Integer> mOnItemClicked;

    public ContextMenuDialogFragment() {
        setAdapter(this);
        mMinWidth = (int) (ScreenUtils.getScreenWidth() / 2.1f);
    }

    public ContextMenuDialogFragment setOnItemClicked(Callback<Integer> onItemClicked) {
        mOnItemClicked = onItemClicked;
        return this;
    }

    public ContextMenuDialogFragment setContextMenuParams(ContextMenuParams params) {
        mParams = params;
        return this;
    }

    public ContextMenuDialogFragment setContextMenuNativeDelegate(ContextMenuNativeDelegate delegate) {
        mDelegate = delegate;
        return this;
    }

    @Override
    protected void initLayoutParams(ViewGroup view) {
        FrameLayout.LayoutParams params = (FrameLayout.LayoutParams)view.getLayoutParams();
        params.height = FrameLayout.LayoutParams.WRAP_CONTENT;
        params.width = mMinWidth;
        view.setFocusableInTouchMode(true);
        view.setFocusable(true);
        view.setClickable(true);
    }

    @Override
    protected DialogAnimator onCreateDialogAnimator(ViewGroup contentView) {
        DialogAnimator animator = super.onCreateDialogAnimator(contentView);
        boolean isFromBottom = !isShowUp;
        if (isFromBottom) {
            mRecycler.getRecyclerView().setLayoutManager(
                    new LinearLayoutManager(context, LinearLayoutManager.VERTICAL, true));
        }
        return animator;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        CardView cardView = findViewById(R.id.cv_container);
        cardView.setElevation(ScreenUtils.dp2pxInt(4));
    }

    @Override
    public void onRecyclerViewCreated(RecyclerView recyclerView, List<MVCListAdapter.ListItem> items) {
        int dp10 = ScreenUtils.dp2pxInt(10);
        int dp18 = ScreenUtils.dp2pxInt(18);
        int textColor = DialogThemeUtils.getMajorTextColor(context);

        mRecycler = new EasyRecycler<>(recyclerView, items);
        mRecycler.setLayoutManager(new LinearLayoutManager(context))
                .onGetChildViewType((list, i) -> list.get(i).type)
                .onGetChildLayoutId(i -> {
                    if (ContextMenuCoordinator.ListItemType.HEADER == i) {
                        return R.layout.context_menu_header;
                    } else if (ContextMenuCoordinator.ListItemType.DIVIDER == i) {
                        return R.layout.app_menu_divider;
                    } else if (ContextMenuCoordinator.ListItemType.CONTEXT_MENU_ITEM == i) {
                        return R.layout._dialog_item_text;
                    } else if (ContextMenuCoordinator.ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON == i) {
                        return R.layout.item_context_menu_share_row;
                    }
                    return 0;
                })
                .onBindViewHolder((holder, list, position, payloads) -> {
                    if (mRecycler.getItemCount() == 1) {
                        holder.setPadding(dp10, dp18, dp10, dp18);
                    } else if (position == 0) {
                        if (isShowUp) {
                            holder.setPadding(dp10, dp10, dp10, dp18);
                        } else {
                            holder.setPadding(dp10, dp18, dp10, dp10);
                        }
                    } else if (position == list.size() - 1) {
                        if (isShowUp) {
                            holder.setPadding(dp10, dp18, dp10, dp10);
                        } else {
                            holder.setPadding(dp10, dp10, dp10, dp18);
                        }
                    } else {
                        holder.setPadding(dp10, dp10, dp10, dp10);
                    }

//                    if (mRecycler.getItemCount() == 1) {
//                        holder.setPadding(dp10, (int) (dp10 * 1.5f), dp10, (int) (dp10 * 1.5f));
//                    } else if (position == 0) {
//                        holder.setPadding(dp10, (int) (dp10 * 1.5f), dp10, dp10);
//                    } else if (position == items.size() - 1) {
//                        if (isReverse) {
//                            holder.setPadding(dp10, dp18, dp10, dp10);
//                        } else {
//                            holder.setPadding(dp10, dp10, dp10, dp18);
//                        }
//                        holder.setPadding(dp10, dp10, dp10, (int) (dp10 * 1.5f));
//                    } else {
//                        holder.setPadding(dp10, dp10, dp10, dp10);
//                    }

                    MVCListAdapter.ListItem item = list.get(position);
                    int viewType = holder.getViewType();
                    if (ContextMenuCoordinator.ListItemType.HEADER == viewType) {
                        ContextMenuHeaderViewBinder.bind(item.model, holder.getItemView());

                        if (!item.model.get(ContextMenuHeaderProperties.HIDE_HEADER_IMAGE)) {
                            if (mParams.isImage()) {
                                final Resources res = getResources();
                                final int imageMaxSize =
                                        res.getDimensionPixelSize(R.dimen.context_menu_header_image_max_size);
                                mDelegate.retrieveImageForContextMenu(
                                        imageMaxSize, imageMaxSize, thumbnail -> {
                                            Bitmap bitmap = getImageWithCheckerBackground(getResources(), thumbnail);
                                            holder.setImageBitmap(R.id.menu_header_image, bitmap);
                                        });
                            } else if (!mParams.isImage() && !mParams.isVideo()) {
                                final int size = item.model.get(ContextMenuHeaderProperties.MONOGRAM_SIZE_PIXEL);
                                FaviconUtil.with(context, mParams.getUrl().getSpec())
                                        .setIconSize(size)
                                        .setCallback(new Callback<Drawable>() {
                                            @Override
                                            public void onResult(Drawable result) {
                                                holder.setImageDrawable(R.id.menu_header_image, result);
                                            }
                                        })
                                        .start();
                            } else if (mParams.isVideo()) {
                                Drawable drawable = ApiCompatibilityUtils.getDrawable(
                                        getResources(), R.drawable.gm_filled_videocam_24);
                                drawable.setColorFilter(
                                        SemanticColorUtils.getDefaultIconColor(context), PorterDuff.Mode.SRC_IN);
                                Bitmap bitmap = Bitmap.createBitmap(drawable.getIntrinsicWidth(),
                                        drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
                                Canvas canvas = new Canvas(bitmap);
                                drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
                                drawable.draw(canvas);
                                holder.setImageBitmap(R.id.menu_header_image, bitmap);
                            }
                        }

                    } else if (ContextMenuCoordinator.ListItemType.DIVIDER == viewType) {
                        holder.setPadding(dp10, 0, dp10, 0);
                    } else if (ContextMenuCoordinator.ListItemType.CONTEXT_MENU_ITEM == viewType) {
                        holder.setTextColor(R.id.tv_text, textColor);
                        holder.setText(R.id.tv_text, item.model.get(TEXT));
                        holder.setVisible(R.id.iv_image, false);
                    } else if (ContextMenuCoordinator.ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON == viewType) {
                        holder.setTextColor(R.id.menu_row_text, textColor);
                        ContextMenuItemWithIconButtonViewBinder.bind(item.model, holder);
                    }

                })
                .onItemClick((holder, v, item) -> {
                    if (mOnItemClicked == null) {
                        return;
                    }

                    int viewType = holder.getViewType();
                    if (viewType == ContextMenuCoordinator.ListItemType.CONTEXT_MENU_ITEM
                            || viewType == ContextMenuCoordinator.ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON) {
                        ZToast.normal((String) item.model.get(TEXT));
                        mOnItemClicked.onResult(item.model.get(MENU_ID));
                    } else {
                        return;
                    }
                    dismiss();
                })
                .build();
    }

    private static class ContextMenuHeaderViewBinder {
        public static void bind(PropertyModel model, View view) {

            String title = model.get(ContextMenuHeaderProperties.TITLE);
            TextView titleText = view.findViewById(R.id.menu_header_title);
            if (TextUtils.isEmpty(title)) {
                titleText.setVisibility(View.GONE);
            } else {
                titleText.setText(title);
                titleText.setVisibility(View.VISIBLE);
                final int maxLines = model.get(ContextMenuHeaderProperties.TITLE_MAX_LINES);
                titleText.setMaxLines(maxLines);
                if (maxLines == Integer.MAX_VALUE) {
                    titleText.setEllipsize(null);
                } else {
                    titleText.setEllipsize(TextUtils.TruncateAt.END);
                }
            }


            TextView urlText = view.findViewById(R.id.menu_header_url);
            CharSequence url = model.get(ContextMenuHeaderProperties.URL);
            if (TextUtils.isEmpty(url)) {
                urlText.setVisibility(View.GONE);
            } else {
                urlText.setText(url);
                urlText.setVisibility(View.VISIBLE);

                final int maxLines = model.get(ContextMenuHeaderProperties.URL_MAX_LINES);
                urlText.setMaxLines(maxLines);
                if (maxLines == Integer.MAX_VALUE) {
                    urlText.setEllipsize(null);
                } else {
                    urlText.setEllipsize(TextUtils.TruncateAt.END);
                }
            }

            view.findViewById(R.id.title_and_url)
                    .setOnClickListener(v -> {
                        if (model.get(ContextMenuHeaderProperties.URL_MAX_LINES) == Integer.MAX_VALUE) {
                            final boolean isTitleEmpty =
                                    TextUtils.isEmpty(model.get(ContextMenuHeaderProperties.TITLE));
                            int maxLines = isTitleEmpty ? 2 : 1;
                            model.set(ContextMenuHeaderProperties.URL_MAX_LINES, maxLines);
                            urlText.setMaxLines(maxLines);
                            urlText.setEllipsize(TextUtils.TruncateAt.END);

                            final boolean isUrlEmpty =
                                    TextUtils.isEmpty(model.get(ContextMenuHeaderProperties.URL));
                            maxLines = isUrlEmpty ? 2 : 1;
                            model.set(ContextMenuHeaderProperties.TITLE_MAX_LINES, maxLines);
                            titleText.setMaxLines(maxLines);
                            titleText.setEllipsize(TextUtils.TruncateAt.END);
                        } else {
                            model.set(ContextMenuHeaderProperties.URL_MAX_LINES, Integer.MAX_VALUE);
                            model.set(ContextMenuHeaderProperties.TITLE_MAX_LINES, Integer.MAX_VALUE);

                            urlText.setMaxLines(Integer.MAX_VALUE);
                            urlText.setEllipsize(null);

                            titleText.setMaxLines(Integer.MAX_VALUE);
                            titleText.setEllipsize(null);
                        }
                    });

            Bitmap bitmap = model.get(ContextMenuHeaderProperties.IMAGE);
            if (bitmap != null) {
                ImageView imageView = view.findViewById(R.id.menu_header_image);
                imageView.setImageBitmap(bitmap);
            }

            final boolean isVisible = model.get(ContextMenuHeaderProperties.CIRCLE_BG_VISIBLE);
            view.findViewById(R.id.circle_background)
                    .setVisibility(isVisible ? View.VISIBLE : View.INVISIBLE);

            @PerformanceHintsObserver.PerformanceClass
            int performanceClass = model.get(ContextMenuHeaderProperties.URL_PERFORMANCE_CLASS);
            view.findViewById(R.id.menu_header_performance_info)
                    .setVisibility(performanceClass == PerformanceHintsObserver.PerformanceClass.PERFORMANCE_FAST
                            ? View.VISIBLE
                            : View.GONE);

            final boolean hideHeaderImage =
                    model.get(ContextMenuHeaderProperties.HIDE_HEADER_IMAGE);
            view.findViewById(R.id.menu_header_image_container)
                    .setVisibility(hideHeaderImage ? View.GONE : View.VISIBLE);

            int maxSizeOverride =
                    model.get(ContextMenuHeaderProperties.OVERRIDE_HEADER_IMAGE_MAX_SIZE_PIXEL);
            if (ContextMenuHeaderProperties.INVALID_OVERRIDE != maxSizeOverride) {
                View image = view.findViewById(R.id.menu_header_image);
                ViewGroup.LayoutParams lp = image.getLayoutParams();
                lp.width = maxSizeOverride;
                lp.height = maxSizeOverride;
                image.setLayoutParams(lp);
            }

            int sizeOverride =
                    model.get(ContextMenuHeaderProperties.OVERRIDE_HEADER_CIRCLE_BG_SIZE_PIXEL);
            if (ContextMenuHeaderProperties.INVALID_OVERRIDE != sizeOverride) {
                View circleBg = view.findViewById(R.id.circle_background);
                ViewGroup.LayoutParams lp = circleBg.getLayoutParams();
                lp.width = sizeOverride;
                lp.height = sizeOverride;
                circleBg.setLayoutParams(lp);
            }

            int marginOverride =
                    model.get(ContextMenuHeaderProperties.OVERRIDE_HEADER_CIRCLE_BG_MARGIN_PIXEL);
            if (ContextMenuHeaderProperties.INVALID_OVERRIDE != marginOverride) {
                View circleBg = view.findViewById(R.id.circle_background);
                ViewGroup.MarginLayoutParams mlp = (ViewGroup.MarginLayoutParams) circleBg.getLayoutParams();
                mlp.setMargins(marginOverride, marginOverride, marginOverride, marginOverride);
                circleBg.setLayoutParams(mlp);
            }
        }
    }

    private static class ContextMenuItemWithIconButtonViewBinder {
        public static void bind(PropertyModel model, EasyViewHolder holder) {

            holder.setText(R.id.menu_row_text, model.get(TEXT));

            Drawable drawable = model.get(BUTTON_IMAGE);
            final ImageView imageView = holder.getImageView(R.id.menu_row_share_icon);
            imageView.setImageDrawable(drawable);
            imageView.setVisibility(drawable != null ? View.VISIBLE : View.GONE);
            final int padding = holder.getContext().getResources().getDimensionPixelSize(
                    R.dimen.context_menu_list_lateral_padding);
            // We don't need extra end padding for the text if the share icon is visible as the icon
            // already has padding.
            holder.setPaddingRelative(R.id.menu_row_text,
                    padding, 0, drawable != null ? 0 : padding, 0);

            holder.getView(R.id.menu_row_share_icon)
                    .setContentDescription(holder.getContext().getString(
                            R.string.accessibility_menu_share_via, model.get(BUTTON_CONTENT_DESC)));

            holder.setOnClickListener(R.id.menu_row_share_icon, model.get(BUTTON_CLICK_LISTENER));



        }
    }

    /**
     * This adds a checkerboard style background to the image.
     * It is useful for the transparent PNGs.
     * @return The given image with the checkerboard pattern in the background.
     */
    private static Bitmap getImageWithCheckerBackground(Resources res, Bitmap image) {
        // 1. Create a bitmap for the checkerboard pattern.
        Drawable drawable =
                ApiCompatibilityUtils.getDrawable(res, R.drawable.checkerboard_background);
        Bitmap tileBitmap = Bitmap.createBitmap(drawable.getIntrinsicWidth(),
                drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
        Canvas tileCanvas = new Canvas(tileBitmap);
        drawable.setBounds(0, 0, drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight());
        drawable.draw(tileCanvas);

        // 2. Create a BitmapDrawable using the checkerboard pattern bitmap.
        BitmapDrawable bitmapDrawable = new BitmapDrawable(res, tileBitmap);
        bitmapDrawable.setTileModeXY(Shader.TileMode.REPEAT, Shader.TileMode.REPEAT);
        bitmapDrawable.setBounds(0, 0, image.getWidth(), image.getHeight());

        // 3. Create a bitmap-backed canvas for the final image.
        Bitmap bitmap =
                Bitmap.createBitmap(image.getWidth(), image.getHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);

        // 4. Paint the checkerboard background into the final canvas
        bitmapDrawable.draw(canvas);

        // 5. Draw the image on top.
        Paint paint = new Paint();
        paint.setAntiAlias(true);
        canvas.drawBitmap(image, new Matrix(), paint);

        return bitmap;
    }

}
