package org.chromium.chrome.browser.shopping.front_door;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.shopping.front_door.ChipProperties.ToggleHandler;
import org.chromium.chrome.browser.shopping.front_door.ShoppingFeedFetcher.CountryCodeProvider;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

public class FrontDoorOnBoardingCoordinator implements ChipsProvider {
    public interface OnboardingObserver {
        public void onDone(List<String> pickedBrandIds);
    }

    private FrontDoorOnBoardingMediator mMediator;

    public FrontDoorOnBoardingCoordinator(Context context, ModalDialogManager modalDialogManager,
            ToggleHandler chipToggleHandler, OnboardCategoryAndBrandProvider dataProvider,
            CountryCodeProvider countryCodeProvider) {
        mMediator = new FrontDoorOnBoardingMediator(
                context, modalDialogManager, chipToggleHandler, dataProvider, countryCodeProvider);
    }

    public boolean hasDoneOnboarding() {
        return mMediator.hasDoneOnboarding();
    }

    public View getOnboardPromoCard(Runnable getStartedCallback) {
        return mMediator.getOnboardPromoCard(getStartedCallback);
    }

    public List<String> getInterestedCategories() {
        return mMediator.getInterestedCategories();
    }

    public List<String> getInterestedBrands() {
        return mMediator.getInterestedBrands();
    }

    public void addObserver(OnboardingObserver observer) {
        mMediator.addObserver(observer);
    }

    // ChipsProvider implementations.
    @Override
    public ListModel<PropertyModel> getChips() {
        return mMediator.getInterestedCategoriesAndBrandsChipModel();
    }
}
