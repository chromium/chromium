// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.shopping;

import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Queue;

public class PrefixTree {

    class Node {
        public final Character nodeChar;
        public Node parent;
        public HashSet<Long> ids;
        public HashMap<Character, Node> children;
        public Node(Character c) {
            nodeChar = c;
        }
    }

    private Node mRoot;

    public PrefixTree() {
        mRoot = new Node(' ');
    }

    public void add(String tag, long id) {
        Node curNode = mRoot;
        for (int i = 0; i < tag.length(); i++) {
            if (curNode.children == null) curNode.children = new HashMap<>();
            if (!curNode.children.containsKey(tag.charAt(i))) {
                Node newNode = new Node(tag.charAt(i));
                newNode.parent = curNode;
                curNode.children.put(tag.charAt(i), newNode);
            }
            curNode = curNode.children.get(tag.charAt(i));
        }
        if (curNode.ids == null) curNode.ids = new HashSet<>();
        curNode.ids.add(id);
    }

    public void remove(long id) {
        HashSet<Node> leavesToRemove = new HashSet<>();
        Queue<Node> queue = new LinkedList<>();
        queue.add(mRoot);
        while (!queue.isEmpty()) {
            Node curNode = queue.poll();
            if (curNode.ids != null && curNode.ids.contains(id)) {
                curNode.ids.remove(id);
                if (curNode.ids.isEmpty()) {
                    curNode.ids = null;
                    if (curNode.children == null) leavesToRemove.add(curNode);
                }
            }
            if (curNode.children != null) {
                for (Map.Entry<Character, Node> entry : curNode.children.entrySet()) {
                    queue.add(entry.getValue());
                }
            }
        }

        for (Node node : leavesToRemove) {
            Node curNode = node;
            while (curNode != null) {
                if (curNode.ids != null && curNode.ids.isEmpty()) curNode.ids = null;
                if (curNode.children != null && curNode.children.isEmpty()) curNode.children = null;

                if (curNode.ids != null || curNode.children != null) break;

                Node parent = curNode.parent;
                curNode.ids = null;
                curNode.children = null;
                curNode.parent = null;

                if (parent != null) {
                    parent.children.remove(curNode.nodeChar);
                }

                curNode = parent;
            }
        }
    }

    public void search(String tag, List<Long> outList) {
        // Find the root first.
        Node searchRoot = mRoot;
        for (int i = 0; i < tag.length(); i++) {
            if (searchRoot.children == null || !searchRoot.children.containsKey(tag.charAt(i))) {
                // Nothing to find with the provided tag.
                return;
            }
            searchRoot = searchRoot.children.get(tag.charAt(i));
        }

        // Collect everything from the search root down.
        Queue<Node> queue = new LinkedList<>();
        queue.add(searchRoot);
        while (!queue.isEmpty()) {
            Node curNode = queue.poll();
            if (curNode.ids != null) outList.addAll(curNode.ids);
            if (curNode.children != null) {
                for (Map.Entry<Character, Node> entry : curNode.children.entrySet()) {
                    queue.add(entry.getValue());
                }
            }
        }
    }
}
