package org.chromium.chrome.browser.shopping_tiles;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

public class ShoppingTasksSection {
    public interface ViewMoreHandler {
        boolean hasMore();
        void viewMore();
    }

    private final Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private final ShoppingTasksDummyProvider mDummyProvider = new ShoppingTasksDummyProvider(2);
    private final Context mContext;
    private final RecyclerView mTaskListRecyclerView;
    private final MVCListAdapter.ModelList mTasksModel;
    private final SimpleRecyclerViewAdapter mAdapter;
    private final ShoppingTasksSectionMediator mMediator;

    public static class ShoppingTaskItemBinder {
        public static void bindTaskItem(
                PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
            if (ShoppingTaskProperties.TASK_NAME == propertyKey) {
                TextView taskNameView = view.findViewById(R.id.task_name);
                taskNameView.setText(model.get(ShoppingTaskProperties.TASK_NAME));
            } else if (ShoppingTaskProperties.PRODUCT_LIST_VIEW_SUPPLIER == propertyKey) {
                assert view instanceof LinearLayout;
                View moreButton = view.findViewById(R.id.more_button);
                int index = view.indexOfChild(moreButton);
                ((LinearLayout) view)
                        .addView(model.get(ShoppingTaskProperties.PRODUCT_LIST_VIEW_SUPPLIER).get(),
                                index,
                                new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                                        ViewGroup.LayoutParams.MATCH_PARENT));
            } else if (ShoppingTaskProperties.VIEW_MORE_HANDLER == propertyKey) {
                View moreButton = view.findViewById(R.id.more_button);
                ViewMoreHandler handler = model.get(ShoppingTaskProperties.VIEW_MORE_HANDLER);
                moreButton.setOnClickListener((v) -> {
                    Log.d("Meil", "Click view more button");
                    handler.viewMore();
                });
            } else if (ShoppingTaskProperties.REFRESH == propertyKey) {
                if (model.get(ShoppingTaskProperties.REFRESH)) {
                    updateProductListView(view);
                    updateViewMoreButton(view, model);
                    model.set(ShoppingTaskProperties.REFRESH, false);
                }
            }
            // TODO(meiliang): Add getView() supplier here to get the view from the component that
            // builds the product recycler view, and add the view from the supplier to the view
            // here. or maybe has the correspondent component in the PropertyModel. Beccause it has
            // to be store somewhere. onRecycle has to remove added view, so add a id to the view,
            // therefore we can findViewById, and remove the view during view recycle.
        }

        private static void updateProductListView(ViewGroup view) {
            view.findViewById(R.id.product_list).requestLayout();
        }

        private static void updateViewMoreButton(ViewGroup view, PropertyModel model) {
            View moreButton = view.findViewById(R.id.more_button);
            ViewMoreHandler handler = model.get(ShoppingTaskProperties.VIEW_MORE_HANDLER);
            moreButton.setEnabled(handler.hasMore());
        }
    }

    public ShoppingTasksSection(Context context,
            Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier, Profile profile) {
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mContext = context;

        mTasksModel = new MVCListAdapter.ModelList();
        mAdapter = new SimpleRecyclerViewAdapter(mTasksModel);

        mTaskListRecyclerView = new RecyclerView(mContext);
        mTaskListRecyclerView.setPadding(0,
                (int) context.getResources().getDimension(R.dimen.default_list_row_padding), 0, 0);
        mTaskListRecyclerView.setHasFixedSize(true);
        mTaskListRecyclerView.setLayoutManager(
                new LinearLayoutManager(mContext, LinearLayoutManager.VERTICAL, false));
        mTaskListRecyclerView.setAdapter(mAdapter);

        mAdapter.registerType(0, parent -> {
            ViewGroup group = (ViewGroup) LayoutInflater.from(context).inflate(
                    R.layout.shopping_task_item_layout, parent, false);
            group.findViewById(R.id.header_menu).setEnabled(false);
            return group;
        }, ShoppingTaskItemBinder::bindTaskItem);

        mMediator = new ShoppingTasksSectionMediator(
                mContext, mEphemeralTabCoordinatorSupplier, mTasksModel, mDummyProvider, profile);
    }

    public View getView() {
        return mTaskListRecyclerView;
    }
}
