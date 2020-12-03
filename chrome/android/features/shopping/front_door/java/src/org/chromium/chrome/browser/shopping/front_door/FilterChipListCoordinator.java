package org.chromium.chrome.browser.shopping.front_door;

import android.content.Context;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.ListModelChangeProcessor.ViewBinder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChipView;

public class FilterChipListCoordinator implements Supplier<View> {
    private final Context mContext;
    private final ChipsProvider mChipsProvider;
    private AutoFlowLayout mView;

    public FilterChipListCoordinator(Context context, ChipsProvider chipsProvider) {
        mContext = context;
        mChipsProvider = chipsProvider;
        mView = (AutoFlowLayout) LayoutInflater.from(context).inflate(
                R.layout.filter_header_item_view, null, false);

        ViewBinder<ListModel<PropertyModel>, AutoFlowLayout> viewBinder =
                new ViewBinder<ListModel<PropertyModel>, AutoFlowLayout>() {
                    @Override
                    public void onItemsInserted(ListModel<PropertyModel> model, AutoFlowLayout view,
                            int index, int count) {
                        Log.e("Meil", "Chips inserted: " + count);
                        for (int i = 0; i < count; i++) {
                            PropertyModel chipModel = model.get(index + i);
                            ChipView chipView = createChipView();
                            bindChipView(chipView, chipModel);
                            mView.addView(chipView);
                        }
                    }

                    @Override
                    public void onItemsRemoved(ListModel<PropertyModel> model, AutoFlowLayout view,
                            int index, int count) {
                        Log.e("Meil", "Chips removed: " + count);
                        mView.removeViews(index, count);
                    }

                    @Override
                    public void onItemsChanged(ListModel<PropertyModel> model, AutoFlowLayout view,
                            int index, int count) {
                        Log.e("Meil", "Chips changed: " + count);
                        for (int i = 0; i < count; i++) {
                            PropertyModel chipModel = model.get(index + i);
                            assert index + i < mView.getChildCount();
                            ChipView chipView = (ChipView) mView.getChildAt(index + i);
                            bindChipView(chipView, chipModel);
                        }
                    }
                };

        ListModelChangeProcessor mcp =
                new ListModelChangeProcessor(mChipsProvider.getChips(), mView, viewBinder);
        mChipsProvider.getChips().addObserver(mcp);

        if (mChipsProvider.getChips().size() > 0) {
            viewBinder.onItemsInserted(
                    mChipsProvider.getChips(), mView, 0, mChipsProvider.getChips().size());
        }
    }

    @Override
    public View get() {
        return mView;
    }

    private void bindChipView(ChipView chipView, PropertyModel model) {
        if (model.get(ChipProperties.TEXT) != null) {
            chipView.getPrimaryTextView().setText(model.get(ChipProperties.TEXT));
        } else {
            chipView.setIconOnly(true);
            chipView.setIcon(R.drawable.ic_edit_24dp, true);
        }
        chipView.setSelected(model.get(ChipProperties.IS_CHECKED));
        chipView.setOnClickListener((v) -> {
            Log.e("Meil", "chip " + model.get(ChipProperties.ID) + " is clicked");
            model.set(ChipProperties.IS_CHECKED, !model.get(ChipProperties.IS_CHECKED));
            chipView.setSelected(model.get(ChipProperties.IS_CHECKED));
            model.get(ChipProperties.TOGGLE_ACTION_HANDLER)
                    .run(model.get(ChipProperties.ID), model.get(ChipProperties.IS_CATEGORY_CHIP),
                            model.get(ChipProperties.IS_CHECKED));
        });
    }

    private ChipView createChipView() {
        return (ChipView) LayoutInflater.from(mContext).inflate(R.layout.filter_chip, mView, false);
    }
}
