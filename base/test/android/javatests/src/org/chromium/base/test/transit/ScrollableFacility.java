// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.PerformException;
import androidx.test.espresso.action.ViewActions;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.ScrollableFacility.Item.Presence;
import org.chromium.base.test.util.RawFailureHandler;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.function.Function;

/**
 * Represents a facility that contains items which may or may not be visible due to scrolling.
 *
 * @param <HostStationT> the type of host {@link Station} this is scoped to.
 */
public abstract class ScrollableFacility<HostStationT extends Station>
        extends Facility<HostStationT> {

    private ArrayList<Item<?>> mItems;

    /** Must populate |items| with the expected items. */
    protected abstract void declareItems(ItemsBuilder items);

    /** Returns the minimum number of items declared expected to be displayed screen initially. */
    protected abstract int getMinimumOnScreenItemCount();

    @CallSuper
    @Override
    public void declareElements(Elements.Builder elements) {
        mItems = new ArrayList<>();
        declareItems(new ItemsBuilder());

        int i = 0;
        int itemsToExpect = getMinimumOnScreenItemCount();
        for (Item<?> item : mItems) {
            // Expect only the first |itemsToExpect| items because of scrolling.
            // Items that should be absent should be checked regardless of position.
            if (item.getPresence() == Presence.ABSENT || i < itemsToExpect) {
                switch (item.mPresence) {
                    case Presence.ABSENT:
                        elements.declareNoView(item.mOnScreenViewMatcher);
                        break;
                    case Presence.PRESENT_AND_ENABLED:
                    case Presence.PRESENT_AND_DISABLED:
                        elements.declareView(item.mViewSpec, item.mViewElementOptions);
                        break;
                    case Presence.MAYBE_PRESENT:
                    case Presence.MAYBE_PRESENT_STUB:
                        // No ViewElements are declared.
                        break;
                }
            }

            i++;
        }
    }

    /**
     * Subclasses' {@link #declareItems(ItemsBuilder)} should declare items through ItemsBuilder.
     */
    public class ItemsBuilder {
        /** Create a new item stub which throws UnsupportedOperationException if selected. */
        public Item<Void> declareStubItem(
                Matcher<View> onScreenViewMatcher, @Nullable Matcher<?> offScreenDataMatcher) {
            Item<Void> item =
                    new Item<>(
                            onScreenViewMatcher,
                            offScreenDataMatcher,
                            Presence.PRESENT_AND_ENABLED,
                            ItemsBuilder::unsupported);
            mItems.add(item);
            return item;
        }

        /** Create a new item which runs |selectHandler| when selected. */
        public <SelectReturnT> Item<SelectReturnT> declareItem(
                Matcher<View> onScreenViewMatcher,
                @Nullable Matcher<?> offScreenDataMatcher,
                Function<ItemOnScreenFacility<SelectReturnT>, SelectReturnT> selectHandler) {
            Item<SelectReturnT> item =
                    new Item<>(
                            onScreenViewMatcher,
                            offScreenDataMatcher,
                            Presence.PRESENT_AND_ENABLED,
                            selectHandler);
            mItems.add(item);
            return item;
        }

        /** Create a new item which transitions to a |DestinationStationT| when selected. */
        public <DestinationStationT extends Station> Item<DestinationStationT> declareItemToStation(
                Matcher<View> onScreenViewMatcher,
                @Nullable Matcher<?> offScreenDataMatcher,
                Callable<DestinationStationT> destinationStationFactory) {
            var item =
                    new Item<DestinationStationT>(
                            onScreenViewMatcher,
                            offScreenDataMatcher,
                            Presence.PRESENT_AND_ENABLED,
                            /* selectHandler= */ null);
            item.setSelectHandler(
                    (itemOnScreenFacility) ->
                            travelToStation(item, itemOnScreenFacility, destinationStationFactory));
            mItems.add(item);
            return item;
        }

        /** Create a new item which enters a |EnteredFacilityT| when selected. */
        public <EnteredFacilityT extends Facility<HostStationT>>
                Item<EnteredFacilityT> declareItemToFacility(
                        Matcher<View> onScreenViewMatcher,
                        @Nullable Matcher<?> offScreenDataMatcher,
                        Callable<EnteredFacilityT> destinationFacilityFactory) {
            final var item =
                    new Item<EnteredFacilityT>(
                            onScreenViewMatcher,
                            offScreenDataMatcher,
                            Presence.PRESENT_AND_ENABLED,
                            /* selectHandler= */ null);
            item.setSelectHandler(
                    (itemOnScreenFacility) ->
                            enterFacility(item, itemOnScreenFacility, destinationFacilityFactory));
            mItems.add(item);
            return item;
        }

        /** Create a new disabled item. */
        public Item<Void> declareDisabledItem(
                Matcher<View> onScreenViewMatcher, @Nullable Matcher<?> offScreenDataMatcher) {
            Item<Void> item =
                    new Item<>(
                            onScreenViewMatcher,
                            offScreenDataMatcher,
                            Presence.PRESENT_AND_DISABLED,
                            null);
            mItems.add(item);
            return item;
        }

        /** Create a new item expected to be absent. */
        public Item<Void> declareAbsentItem(
                Matcher<View> onScreenViewMatcher, @Nullable Matcher<?> offScreenDataMatcher) {
            Item<Void> item =
                    new Item<>(onScreenViewMatcher, offScreenDataMatcher, Presence.ABSENT, null);
            mItems.add(item);
            return item;
        }

        /** Create a new item which may or may not be present. */
        public <SelectReturnT> Item<SelectReturnT> declarePossibleItem(
                Matcher<View> onScreenViewMatcher,
                @Nullable Matcher<?> offScreenDataMatcher,
                Function<ItemOnScreenFacility<SelectReturnT>, SelectReturnT> selectHandler) {
            Item<SelectReturnT> item =
                    new Item<>(
                            onScreenViewMatcher,
                            offScreenDataMatcher,
                            Presence.MAYBE_PRESENT,
                            selectHandler);
            mItems.add(item);
            return item;
        }

        /** Create a new item stub which may or may not be present. */
        public <SelectReturnT> Item<SelectReturnT> declarePossibleStubItem() {
            Item<SelectReturnT> item =
                    new Item<>(
                            /* onScreenViewMatcher= */ null,
                            /* offScreenDataMatcher= */ null,
                            Presence.MAYBE_PRESENT_STUB,
                            /* selectHandler= */ null);
            mItems.add(item);
            return item;
        }

        private static <HostStationT extends Station> Void unsupported(
                ScrollableFacility<HostStationT>.ItemOnScreenFacility<Void> itemOnScreen) {
            // Selected an item created with newStubItem().
            // Use newItemToStation(), newItemToFacility() or newItem() to declare expected behavior
            // when this item is selected.
            throw new UnsupportedOperationException(
                    "This item is a stub and has not been bound to a select handler.");
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

        /** Whether the item is expected to be present and enabled. */
        @IntDef({
            Presence.ABSENT,
            Presence.PRESENT_AND_ENABLED,
            Presence.PRESENT_AND_DISABLED,
            Presence.MAYBE_PRESENT,
            Presence.MAYBE_PRESENT_STUB,
        })
        @Retention(RetentionPolicy.SOURCE)
        public @interface Presence {
            // Item must not be present.
            int ABSENT = 0;

            // Item must be present and enabled.
            int PRESENT_AND_ENABLED = 1;

            // Item must be present and disabled.
            int PRESENT_AND_DISABLED = 2;

            // No expectations on item being present or enabled.
            int MAYBE_PRESENT = 3;

            // No expectations on item being present or enabled, and select trigger is not
            // implemented. Some optimizations can be made.
            int MAYBE_PRESENT_STUB = 4;
        }

        protected final @Nullable Matcher<View> mOnScreenViewMatcher;
        protected final @Nullable Matcher<?> mOffScreenDataMatcher;
        protected final @Presence int mPresence;
        protected final @Nullable ViewSpec mViewSpec;
        protected final @Nullable ViewElement.Options mViewElementOptions;
        protected @Nullable Function<ItemOnScreenFacility<SelectReturnT>, SelectReturnT>
                mSelectHandler;

        /**
         * Use one of {@link ScrollableFacility.ItemsBuilder}'s methods to instantiate:
         *
         * <ul>
         *   <li>{@link ItemsBuilder#declareItem(Matcher, Matcher, Function)}
         *   <li>{@link ItemsBuilder#declareItemToFacility(Matcher, Matcher, Callable)}
         *   <li>{@link ItemsBuilder#declareItemToStation(Matcher, Matcher, Callable)}
         *   <li>{@link ItemsBuilder#declareDisabledItem(Matcher, Matcher)}
         *   <li>{@link ItemsBuilder#declareAbsentItem(Matcher, Matcher)}
         *   <li>{@link ItemsBuilder#declareStubItem(Matcher, Matcher)}
         *   <li>{@link ItemsBuilder#declarePossibleItem(Matcher, Matcher, Function)}
         *   <li>{@link ItemsBuilder#declarePossibleStubItem()}
         * </ul>
         */
        protected Item(
                @Nullable Matcher<View> onScreenViewMatcher,
                @Nullable Matcher<?> offScreenDataMatcher,
                @Presence int presence,
                @Nullable
                        Function<ItemOnScreenFacility<SelectReturnT>, SelectReturnT>
                                selectHandler) {
            mPresence = presence;
            mOnScreenViewMatcher = onScreenViewMatcher;
            mOffScreenDataMatcher = offScreenDataMatcher;
            mSelectHandler = selectHandler;

            switch (mPresence) {
                case Presence.ABSENT, Presence.MAYBE_PRESENT_STUB:
                    mViewSpec = null;
                    mViewElementOptions = null;
                    break;
                case Presence.PRESENT_AND_ENABLED:
                case Presence.MAYBE_PRESENT:
                    mViewSpec = viewSpec(mOnScreenViewMatcher);
                    mViewElementOptions = ViewElement.Options.DEFAULT;
                    break;
                case Presence.PRESENT_AND_DISABLED:
                    mViewSpec = viewSpec(mOnScreenViewMatcher);
                    mViewElementOptions = ViewElement.expectDisabledOption();
                    break;
                default:
                    mViewSpec = null;
                    mViewElementOptions = null;
                    assert false;
            }
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
        public ItemOnScreenFacility<SelectReturnT> scrollTo() {
            assert mPresence != Presence.ABSENT;

            // Could in theory try to scroll to a stub, but not supporting this prevents the
            // creation of a number of objects that are likely not going to be used.
            assert mPresence != Presence.MAYBE_PRESENT_STUB;

            ItemOnScreenFacility<SelectReturnT> focusedItem = new ItemOnScreenFacility<>(this);

            try {
                onView(mOnScreenViewMatcher)
                        .withFailureHandler(RawFailureHandler.getInstance())
                        .check(matches(isCompletelyDisplayed()));
                return mHostStation.enterFacilitySync(focusedItem, /* trigger= */ null);
            } catch (AssertionError | NoMatchingViewException e) {
                return mHostStation.enterFacilitySync(focusedItem, this::triggerScrollTo);
            }
        }

        protected void setSelectHandler(
                Function<ItemOnScreenFacility<SelectReturnT>, SelectReturnT> selectHandler) {
            assert mSelectHandler == null;
            mSelectHandler = selectHandler;
        }

        public @Presence int getPresence() {
            return mPresence;
        }

        public ViewSpec getViewSpec() {
            return mViewSpec;
        }

        public ViewElement.Options getViewElementOptions() {
            return mViewElementOptions;
        }

        protected Function<ItemOnScreenFacility<SelectReturnT>, SelectReturnT> getSelectHandler() {
            return mSelectHandler;
        }

        private void triggerScrollTo() {
            if (mOffScreenDataMatcher != null) {
                // If there is a data matcher, use it to scroll as the item might be in a
                // RecyclerView.
                try {
                    onData(mOffScreenDataMatcher).perform(ViewActions.scrollTo());
                } catch (PerformException performException) {
                    throw TravelException.newTravelException(
                            String.format(
                                    "Could not scroll using data matcher %s",
                                    mOffScreenDataMatcher),
                            performException);
                }
            } else {
                // If there is no data matcher, use the ViewMatcher to scroll as the item should be
                // created but not displayed.
                try {
                    onView(mOnScreenViewMatcher).perform(ViewActions.scrollTo());
                } catch (PerformException performException) {
                    throw TravelException.newTravelException(
                            String.format(
                                    "Could not scroll using view matcher %s", mOnScreenViewMatcher),
                            performException);
                }
            }
        }
    }

    private <EnteredFacilityT extends Facility> EnteredFacilityT enterFacility(
            Item<EnteredFacilityT> item,
            ItemOnScreenFacility<EnteredFacilityT> itemOnScreenFacility,
            Callable<EnteredFacilityT> destinationFactory) {
        EnteredFacilityT destination;
        try {
            destination = destinationFactory.call();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        return mHostStation.swapFacilitySync(
                List.of(this, itemOnScreenFacility), destination, item.getViewSpec()::click);
    }

    private <DestinationStationT extends Station> DestinationStationT travelToStation(
            Item<DestinationStationT> item,
            ItemOnScreenFacility<DestinationStationT> itemOnScreenFacility,
            Callable<DestinationStationT> destinationFactory) {
        DestinationStationT destination;
        try {
            destination = destinationFactory.call();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        return mHostStation.travelToSync(destination, item.getViewSpec()::click);
    }

    /** Get all {@link Item}s declared in this {@link ScrollableFacility}. */
    public List<Item<?>> getItems() {
        return mItems;
    }

    /**
     * A facility representing an item inside a {@link ScrollableFacility} shown on the screen.
     *
     * @param <SelectReturnT> the return type of the |selectHandler|.
     */
    public class ItemOnScreenFacility<SelectReturnT> extends Facility<HostStationT> {

        protected final Item<SelectReturnT> mItem;

        protected ItemOnScreenFacility(Item<SelectReturnT> item) {
            mItem = item;
        }

        @Override
        public void declareElements(Elements.Builder elements) {
            elements.declareView(mItem.getViewSpec(), mItem.getViewElementOptions());
        }

        /** Select the item and trigger its |selectHandler|. */
        public SelectReturnT select() {
            if (mItem.getPresence() == Presence.ABSENT) {
                throw new IllegalStateException("Cannot click on an absent item");
            }
            if (mItem.getPresence() == Presence.PRESENT_AND_DISABLED) {
                throw new IllegalStateException("Cannot click on a disabled item");
            }

            try {
                return mItem.getSelectHandler().apply(this);
            } catch (Exception e) {
                throw TravelException.newTravelException("Select handler threw an exception:", e);
            }
        }

        /** Returns the {@link Item} that is on the screen. */
        public Item<SelectReturnT> getItem() {
            return mItem;
        }

        /** Returns a {@link Transition.Trigger} to click the item. */
        public Transition.Trigger clickTrigger() {
            return getItem().getViewSpec()::click;
        }
    }
}
