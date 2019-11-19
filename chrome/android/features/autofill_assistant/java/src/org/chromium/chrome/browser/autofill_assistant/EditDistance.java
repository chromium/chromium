// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.IntDef;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.ui.modelutil.ListModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * The <a href="https://en.wikipedia.org/wiki/Edit_distance">edit distance</a> is a mean of
 * quantifying the distance/cost of two sequences (e.g. words or lists) by computing the minimum
 * number of operations required to transform a source sequence into a target sequence.
 *
 * <p>One of the most popular example is the <a
 * href="https://en.wikipedia.org/wiki/Levenshtein_distance">Levenshtein Distance</a> whose
 * supported operations are item insertion, item deletion and item substitution.
 */
public final class EditDistance {
    /**
     * An {@link Equivalence} class to test for equivalence between 2 items of the same type.
     */
    interface Equivalence<T> {
        /** Return whether {@code a} and {@code b} are equivalent. */
        boolean apply(T a, T b);
    }

    /**
     * Apply the set of operations with minimum cost necessary to transform {@code source} into
     * {@code target} using the sets of edit operations defined by Levenshtein (insertion, deletion
     * and substitution).
     *
     * <p>Insertions and deletions always have a cost of 1. Substituting a value by another costs 0
     * if both values are equivalent according to {@code equivalence}, and it costs 1 otherwise.
     */
    public static <T> void transform(
            ListModel<T> source, List<T> target, Equivalence<T> equivalence) {
        applyOperations(source, target, computeOperations(source, target, equivalence));
    }

    /**
     * Compute the minimum cost set of operations to perform on {@code source} to transform it into
     * {@code target}. Elements are compared using {@code equivalence} when testing for equivalence.
     *
     * <p>The positions used in the resulting list of operations are relative to the {@code source}
     * sequence. For instance, let's say we have:
     * <pre>
     *     source = [1, 2, 3]
     *     target = [2, 4, 3]
     * </pre>
     *
     * then a minimum cost set of operations is:
     * <pre>
     *   [
     *     Substitution(index = 2, value = 3)],    // cost = 0
     *     Insertion(index = 2, value = 4),        // cost = 1    total cost = 2
     *     Deletion(index = 0)                     // cost = 1
     *   ]
     * </pre>
     *
     * <p>Note that there can be multiple solutions and there is currently no warranty on which
     * one will be returned. For instance, another minimal set of operations for this example could
     * be:
     * <pre>
     *     [
     *       Substitution(index = 0, value = 2),   // cost = 1
     *       Substitution(index = 1, value = 4)],  // cost = 1    total cost = 2
     *       Substitution(index = 2, value = 3)    // cost = 0
     *     ]
     * </pre>
     */
    private static <T> List<Operation> computeOperations(
            ListModel<T> source, List<T> target, Equivalence<T> equivalence) {
        int n = source.size();
        int m = target.size();

        // cache[i][j] stores the Levenshtein distance between source[0;i[ and target[0;j[, as
        // well as references to other cache entries and an operation (insert, substitute, delete)
        // to be able to reconstruct the optimal set of operations to perform on source such that it
        // is equal to target.
        CacheEntry[][] cache = new CacheEntry[n + 1][m + 1];

        // Transforming empty list into empty list costs 0.
        cache[0][0] = new CacheEntry(0, null, null);

        // Cache[i][0].cost is the cost of transforming source[0;i[ into the empty list.
        for (int i = 1; i <= n; i++) {
            cache[i][0] = new CacheEntry(i, Operation.deletion(i - 1), cache[i - 1][0]);
        }

        // Cache[0][j].cost is the cost of transforming the empty list into target[0;j[.
        for (int j = 1; j <= m; j++) {
            cache[0][j] = new CacheEntry(j, Operation.insertion(0, j - 1), cache[0][j - 1]);
        }

        for (int i = 1; i <= n; i++) {
            for (int j = 1; j <= m; j++) {
                T aValue = source.get(i - 1);
                T bValue = target.get(j - 1);

                // 1) Substitution.
                CacheEntry previousEntry = cache[i - 1][j - 1];
                Operation operation = Operation.substitution(i - 1, j - 1);
                int substitutionCost = equivalence.apply(aValue, bValue) ? 0 : 1;
                int cost = previousEntry.mCost + substitutionCost;

                // 2) Deletion.
                CacheEntry deletionEntry = cache[i - 1][j];
                if (deletionEntry.mCost + 1 < cost) {
                    cost = deletionEntry.mCost + 1;
                    operation = Operation.deletion(i - 1);
                    previousEntry = deletionEntry;
                }

                // 3) Insertion.
                CacheEntry insertionEntry = cache[i][j - 1];
                if (insertionEntry.mCost + 1 < cost) {
                    cost = insertionEntry.mCost + 1;
                    operation = Operation.insertion(i, j - 1);
                    previousEntry = insertionEntry;
                }
                cache[i][j] = new CacheEntry(cost, operation, previousEntry);
            }
        }

        // Return the list of operations.
        ArrayList<Operation> result = new ArrayList<>();
        CacheEntry current = cache[n][m];
        while (current != null) {
            if (current.mOperation != null) {
                result.add(current.mOperation);
            }
            current = current.mPreviousEntry;
        }
        sortOperations(result);

        // TODO(crbug.com/806868): We might want to merge some operations (e.g. insertion of items
        // at the same index or removal of a range) instead of inserting and removing items one by
        // one.
        return result;
    }

    /**
     * Sort the operations such that we can apply them in the resulting order.
     *
     * <p>The logic is the following:
     *  1) Apply all substitutions first, in any order.
     *  2) Apply insertions and deletions altogether, such that:
     *   - deletions and insertions at index i will be performed before deletions and insertions at
     *     index j if i > j.
     *   - deletions at index i will be performed insertions at index i.
     *   - insertion at index i of target[j] will be performed before insertion at index i of
     *     target[k] if j > k.
     */
    private static void sortOperations(List<Operation> operations) {
        Collections.sort(operations, (a, b) -> {
            // We perform substitutions first.
            int c = ApiCompatibilityUtils.compareBoolean(
                    b.mType == Operation.Type.SUBSTITUTION, a.mType == Operation.Type.SUBSTITUTION);
            if (c != 0) {
                return c;
            }

            // We apply deletions and insertions in decreasing index order.
            c = Integer.compare(b.mSourceIndex, a.mSourceIndex);
            if (c != 0) {
                return c;
            }

            // When the indices are equal, we first perform the deletion at that index.
            c = ApiCompatibilityUtils.compareBoolean(
                    b.mType == Operation.Type.DELETION, a.mType == Operation.Type.DELETION);
            if (c != 0) {
                return c;
            }

            // If we need to insert multiple values at the same index, we do it in decreasing
            // order of target index.
            return Integer.compare(b.mTargetIndex, b.mTargetIndex);
        });
    }

    /**
     * Apply operations on {@code source}.
     */
    private static <T> void applyOperations(
            ListModel<T> source, List<T> target, List<Operation> operations) {
        for (int i = 0; i < operations.size(); i++) {
            Operation operation = operations.get(i);
            switch (operation.mType) {
                case Operation.Type.INSERTION:
                    source.add(operation.mSourceIndex, target.get(operation.mTargetIndex));
                    break;
                case Operation.Type.SUBSTITUTION:
                    source.update(operation.mSourceIndex, target.get(operation.mTargetIndex));
                    break;
                case Operation.Type.DELETION:
                    source.removeAt(operation.mSourceIndex);
                    break;
            }
        }
    }

    /** An operation that can be applied to a sequence. */
    private static class Operation {
        @IntDef({Type.INSERTION, Type.SUBSTITUTION, Type.DELETION})
        @Retention(RetentionPolicy.SOURCE)
        @interface Type {
            int INSERTION = 0;
            int SUBSTITUTION = 1;
            int DELETION = 2;
        }

        /** The type of this operation. */
        final @Type int mType;

        /**
         * An index from the source sequence that represents:
         *  - at which index a value should be inserted if mType == INSERTION.
         *  - at which index we should change the value if mType == SUBSTITUTION.
         *  - at which index a value should be deleted if mType == DELETION.
         */
        final int mSourceIndex;

        /**
         * An index from the target sequence that represents:
         *  - the index of the value to insert in the source sequence if mType == INSERTION.
         *  - the index of the new value if mType == SUBSTITUTION.
         *  - (invalid index = -1) if mType == DELETION.
         */
        final int mTargetIndex;

        Operation(@Type int type, int sourceIndex, int targetIndex) {
            this.mType = type;
            this.mSourceIndex = sourceIndex;
            this.mTargetIndex = targetIndex;
        }

        static Operation insertion(int sourceIndex, int targetIndex) {
            return new Operation(Type.INSERTION, sourceIndex, targetIndex);
        }

        static Operation substitution(int sourceIndex, int targetIndex) {
            return new Operation(Type.SUBSTITUTION, sourceIndex, targetIndex);
        }

        static Operation deletion(int sourceIndex) {
            return new Operation(Type.DELETION, sourceIndex, -1);
        }
    }

    /** Util class used to store the result of a DP sub problem in #computeOperations().*/
    private static class CacheEntry {
        final int mCost;
        final Operation mOperation;
        final CacheEntry mPreviousEntry;

        CacheEntry(int cost, Operation operation, CacheEntry previousEntry) {
            this.mCost = cost;
            this.mOperation = operation;
            this.mPreviousEntry = previousEntry;
        }
    }
}
