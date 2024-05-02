// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;

import static org.chromium.base.test.transit.ViewElement.sharedViewElement;

import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.action.ViewActions;

import org.hamcrest.Matcher;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * Represents a facility that contains items which may or may not be visible due to scrolling.
 *
 * @param <HostStationT> the type of host {@link Station} this is scoped to.
 */
public abstract class ScrollableFacility<HostStationT extends Station>
        extends Facility<HostStationT> {

    public ScrollableFacility(HostStationT station) {
        super(station);
    }

    /** Must populate |items| with the expected items. */
    protected abstract void declareItems(List<Item<?>> items);

    /** Returns the minimum number of items declared expected to be displayed screen initially. */
    protected abstract int getMinimumOnScreenItemCount();

    /** Create a new item stub which throws UnsupportedOperationException if selected. */
    public Item<Void> newStubItem(
            Matcher<View> onScreenViewMatcher, Matcher<?> offScreenDataMatcher) {
        return new Item<>(
                onScreenViewMatcher,
                offScreenDataMatcher,
                /* present= */ true,
                /* enabled= */ true,
                ScrollableFacility::unsupported);
    }

    /** Create a new item which runs |selectHandler| when selected. */
    public <SelectReturnT> Item<SelectReturnT> newItem(
            Matcher<View> onScreenViewMatcher,
            Matcher<?> offScreenDataMatcher,
            Callable<SelectReturnT> selectHandler) {
        return new Item<>(
                onScreenViewMatcher,
                offScreenDataMatcher,
                /* present= */ true,
                /* enabled= */ true,
                selectHandler);
    }

    /** Create a new item which transitions to a |DestinationStationT| when selected. */
    public <DestinationStationT extends Station> Item<DestinationStationT> newItemToStation(
            Matcher<View> onScreenViewMatcher,
            Matcher<?> offScreenDataMatcher,
            Callable<DestinationStationT> destinationStationFactory) {
        var item =
                new Item<DestinationStationT>(
                        onScreenViewMatcher,
                        offScreenDataMatcher,
                        /* present= */ true,
                        /* enabled= */ true,
                        /* selectHandler= */ null);
        item.setSelectHandler(() -> travelToStation(item, destinationStationFactory));
        return item;
    }

    /** Create a new item which enters a |EnteredFacilityT| when selected. */
    public <EnteredFacilityT extends Facility<HostStationT>>
            Item<EnteredFacilityT> newItemToFacility(
                    Matcher<View> onScreenViewMatcher,
                    Matcher<?> offScreenDataMatcher,
                    Callable<EnteredFacilityT> destinationFacilityFactory) {
        final var item =
                new Item<EnteredFacilityT>(
                        onScreenViewMatcher,
                        offScreenDataMatcher,
                        /* present= */ true,
                        /* enabled= */ true,
                        /* selectHandler= */ null);
        item.setSelectHandler(() -> enterFacility(item, destinationFacilityFactory));
        return item;
    }

    /** Create a new disabled item. */
    public Item<Void> newDisabledItem(
            Matcher<View> onScreenViewMatcher, Matcher<?> offScreenDataMatcher) {
        return new Item<>(
                onScreenViewMatcher,
                offScreenDataMatcher,
                /* present= */ true,
                /* enabled= */ false,
                null);
    }

    /** Create a new item expected to be absent. */
    public Item<Void> newAbsentItem(
            Matcher<View> onScreenViewMatcher, Matcher<?> offScreenDataMatcher) {
        return new Item<>(
                onScreenViewMatcher,
                offScreenDataMatcher,
                /* present= */ false,
                /* enabled= */ false,
                null);
    }

    private static Void unsupported() {
        // Selected an item created with newStubItem().
        // Use newItemToStation(), newItemToFacility() or newItem() to declare expected behavior
        // when this item is selected.
        throw new UnsupportedOperationException(
                "This item is a stub and has not been bound to a select handler.");
    }

    @CallSuper
    @Override
    public void declareElements(Elements.Builder elements) {
        List<Item<?>> items = new ArrayList<>();
        declareItems(items);

        int i = 0;
        int itemsToExpect = getMinimumOnScreenItemCount();
        for (Item<?> item : items) {
            // Expect only the first |itemsToExpect| items because of scrolling.
            // Items that should be absent should be checked regardless of position.
            if (!item.isPresent() || i < itemsToExpect) {
                item.declareViewElement(elements);
            }

            i++;
        }
    }

    /**
     * Represents an item in a specific {@link ScrollableFacility}.
     *
     * <p>{@link ScrollableFacility} subclasses should use these to represent their items.
     *
     * @param <SelectReturnT> the return type of the |selectHandler|.
     */
    public class Item<SelectReturnT> {
        protected final Matcher<View> mOnScreenViewMatcher;
        protected final Matcher<?> mOffScreenDataMatcher;
        protected final boolean mPresent;
        protected final boolean mEnabled;
        protected final ViewElement mViewElement;
        protected Callable<SelectReturnT> mSelectHandler;

        /**
         * Use one of {@link ScrollableFacility}'s factory methods to instantiate:
         *
         * <ul>
         *   <li>{@link #newStubItem(Matcher, Matcher)}
         *   <li>{@link #newItem(Matcher, Matcher, Callable)}
         *   <li>{@link #newItemToFacility(Matcher, Matcher, Callable)}
         *   <li>{@link #newItemToStation(Matcher, Matcher, Callable)}
         *   <li>{@link #newDisabledItem(Matcher, Matcher)}
         *   <li>{@link #newAbsentItem(Matcher, Matcher)}
         * </ul>
         */
        protected Item(
                Matcher<View> onScreenViewMatcher,
                Matcher<?> offScreenDataMatcher,
                boolean present,
                boolean enabled,
                @Nullable Callable<SelectReturnT> selectHandler) {
            mPresent = present;
            mEnabled = enabled;
            mOnScreenViewMatcher = onScreenViewMatcher;
            if (mPresent) {
                if (mEnabled) {
                    mViewElement = sharedViewElement(mOnScreenViewMatcher);
                } else {
                    mViewElement =
                            sharedViewElement(
                                    mOnScreenViewMatcher,
                                    ViewElement.newOptions().expectDisabled().build());
                }
            } else {
                mViewElement = null;
            }
            mOffScreenDataMatcher = offScreenDataMatcher;
            mSelectHandler = selectHandler;
        }

        /**
         * Select the item, scrolling to it if necessary.
         *
         * @return the return value of the |selectHandler|. e.g. a Facility or Station.
         */
        public SelectReturnT scrollToAndSelect() {
            return scrollTo().select();
        }

        /**
         * Scroll to the item if necessary.
         *
         * @return a ItemScrolledTo facility representing the item on the screen, which runs the
         *     |selectHandler| when selected.
         */
        public ItemOnScreenFacility<HostStationT, SelectReturnT> scrollTo() {
            ItemOnScreenFacility<HostStationT, SelectReturnT> focusedItem =
                    new ItemOnScreenFacility<>(mHostStation, this);
            return Facility.enterSync(focusedItem, this::maybeScrollTo);
        }

        protected void setSelectHandler(Callable<SelectReturnT> selectHandler) {
            assert mSelectHandler == null;
            mSelectHandler = selectHandler;
        }

        protected void declareViewElement(Elements.Builder elements) {
            if (mViewElement != null) {
                elements.declareView(mViewElement);
            } else {
                elements.declareNoView(mOnScreenViewMatcher);
            }
        }

        protected boolean isPresent() {
            return mPresent;
        }

        protected boolean isEnabled() {
            return mEnabled;
        }

        protected ViewElement getViewElement() {
            return mViewElement;
        }

        protected Callable<SelectReturnT> getSelectHandler() {
            return mSelectHandler;
        }

        private void maybeScrollTo() {
            try {
                onView(mOnScreenViewMatcher).check(matches(isCompletelyDisplayed()));
            } catch (AssertionError | NoMatchingViewException e) {
                onData(mOffScreenDataMatcher).perform(ViewActions.scrollTo());
            }
        }
    }

    private <EnteredFacilityT extends Facility> EnteredFacilityT enterFacility(
            Item<EnteredFacilityT> item, Callable<EnteredFacilityT> mDestinationFactory) {
        EnteredFacilityT destination;
        try {
            destination = mDestinationFactory.call();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        return Facility.enterSync(destination, () -> item.getViewElement().perform(click()));
    }

    private <DestinationStationT extends Station> DestinationStationT travelToStation(
            Item<DestinationStationT> item, Callable<DestinationStationT> mDestinationFactory) {
        DestinationStationT destination;
        try {
            destination = mDestinationFactory.call();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        return Trip.travelSync(
                mHostStation, destination, () -> item.getViewElement().perform(click()));
    }
}
