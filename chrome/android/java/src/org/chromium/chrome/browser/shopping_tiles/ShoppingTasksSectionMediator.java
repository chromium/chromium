package org.chromium.chrome.browser.shopping_tiles;

import static org.chromium.chrome.browser.shopping_tiles.ShoppingTaskProperties.PRODUCT_LIST_VIEW_SUPPLIER;
import static org.chromium.chrome.browser.shopping_tiles.ShoppingTaskProperties.REFRESH;
import static org.chromium.chrome.browser.shopping_tiles.ShoppingTaskProperties.TASK_NAME;
import static org.chromium.chrome.browser.shopping_tiles.ShoppingTaskProperties.VIEW_MORE_HANDLER;

import android.content.Context;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.shopping_tiles.ShoppingProductListCoordinator.ListType;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

class ShoppingTasksSectionMediator {
    private final Context mContext;
    private final Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;

    private final MVCListAdapter.ModelList mModel;
    private final ShoppingTasksProvider mTasksProvider;
    private final Profile mProfile;

    ShoppingTasksSectionMediator(Context context,
            Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            MVCListAdapter.ModelList tasksModel, ShoppingTasksProvider tasksProvider,
            Profile profile) {
        mContext = context;
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mModel = tasksModel;
        mTasksProvider = tasksProvider;
        mProfile = profile;

        addTasksToModel();
    }

    private void addTasksToModel() {
        for (ShoppingTask task : mTasksProvider.getTasks()) {
            ShoppingProductListCoordinator productListBuilder =
                    new ShoppingProductListCoordinator(mContext, mEphemeralTabCoordinatorSupplier,
                            mProfile, ListType.GRID, null, null, null, null, null);
            productListBuilder.setProductList(task.getProductList());
            PropertyModel taskModel = new PropertyModel.Builder(ShoppingTaskProperties.ALL_KEYS)
                                              .with(TASK_NAME, task.name)
                                              .with(PRODUCT_LIST_VIEW_SUPPLIER, productListBuilder)
                                              .with(VIEW_MORE_HANDLER, productListBuilder)
                                              .build();
            mModel.add(new SimpleRecyclerViewAdapter.ListItem(0, taskModel));
            final int index = mModel.size() - 1;
            productListBuilder.addObserver(() -> { productListUpdated(index); });
        }
    }

    private void productListUpdated(int index) {
        mModel.get(index).model.set(
                VIEW_MORE_HANDLER, mModel.get(index).model.get(VIEW_MORE_HANDLER));
        mModel.get(index).model.set(REFRESH, true);
    }
}
