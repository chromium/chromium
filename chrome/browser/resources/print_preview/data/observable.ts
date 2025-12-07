// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Non-Polymer subproperty observation implementation, to facilitate Lit
// migrations for WebUIs that heavily depend on Polymer's subproperty
// observation.
//
// High level design:
//  - buildProxy(): Helper function to wrap the object to be observed with an ES
//    Proxy. The function calls itself recursively (lazily) to create as many
//    Proxy instances as needed to intercept all sub-objects/arrays. 'callback'
//    is invoked whenever any path changes. This function is the primitive upon
//    the rest of this approach is based on.
//  - ObserverTree: A tree data structure of ObserverNode instances, to keep
//    track of observers that are registered for each node of the original
//    object.
//  - Observable: The class to be used by client code to observe an object,
//    leverages buildProxy and ObserverTree internally.
//
//  The format of change notifications is following Polymer observers
//  notifications (with extensive test coverage to ensure compatibility), to
//  make it easier to migrate Polymer observers to Observable observers.
//
//  Note: Array push() and splice() notifications are not implemented yet, will
//  be implemented if/when the need arises.

import {assert} from 'chrome://resources/js/assert.js';

export type ChangeCallback =
    (newValue: any, previousValue: any, path: string) => void;

export interface WildcardChangeRecord {
  path: string;
  value: any;
  base: Record<string, any>;
}

type WildcardChangeCallback = (change: WildcardChangeRecord) => void;

function buildProxy(
    obj: Record<string, any>, callback: ChangeCallback, path: string[],
    proxyCache: WeakMap<object, object>): Record<string, any> {
  function getPath(prop: string): string {
    return path.slice(1).concat(prop).join('.');
  }

  return new Proxy(obj, {
    get(target: Record<string, any>, prop: string) {
      const value = target[prop];

      if (value && typeof value === 'object' &&
          ['Array', 'Object'].includes(value.constructor.name)) {
        let proxy = proxyCache.get(value) || null;
        if (proxy === null) {
          proxy = buildProxy(value, callback, path.concat(prop), proxyCache);
          proxyCache.set(value, proxy);
        }
        return proxy;
      }

      return value;
    },

    set(target: Record<string, any>, prop: string, value: any) {
      const previousValue = target[prop];

      if (previousValue === value) {
        return true;
      }

      target[prop] = value;
      callback(value, previousValue, getPath(prop));
      return true;
    },
  });
}

function getValueAtPath(pathParts: string[], obj: Record<string, any>) {
  let result: Record<string, any> = obj;
  let counter = pathParts.length;
  while (counter > 1) {
    const current = pathParts[pathParts.length - counter--]!;
    result = result[current];
  }
  return result[pathParts.at(-1)!];
}

export function setValueAtPath(
    pathParts: string[], obj: Record<string, any>, value: any) {
  let parent: Record<string, any> = obj;
  let counter = pathParts.length;
  while (counter > 1) {
    const current = pathParts[pathParts.length - counter--]!;
    parent = parent[current];
  }

  parent[pathParts.at(-1)!] = value;
}

interface ObserverEntry {
  id: number;
  callback: ChangeCallback|WildcardChangeCallback;
  isWildcard: boolean;
}

interface ObserverNode {
  parent?: ObserverNode;
  key: string;
  observers?: Set<ObserverEntry>;
  children?: Map<string, ObserverNode>;
}

// A tree data structure to keep track of observers for a nested data object. It
// replicates the behavior of Polymer observers.
class ObserverTree {
  root: ObserverNode = {key: ''};

  // Internal book keeping to make it easy to remove observers using an ID.
  private nextObserverId_: number = -1;
  private observers_: Map<number, ObserverNode> = new Map();

  getNode(path: string, create: boolean = false): ObserverNode|null {
    const pathParts = path.split('.');
    let node: ObserverNode|null = this.root;

    while (pathParts.length > 0 && node !== null) {
      const currentPart = pathParts.shift()!;
      if (create && !node.children) {
        node.children = new Map();
      }

      let child: ObserverNode|null = node.children?.get(currentPart) || null;

      if (create && child === null) {
        child = {parent: node, key: currentPart};
        node.children!.set(child.key, child);
      }

      node = child;
    }

    return node;
  }

  // Traverses all nodes between the root of the tree and the node corresponding
  // to 'path' (including that node as well).
  traversePath(
      path: string,
      callback:
          (node: ObserverNode, isLast: boolean, pathParts: string[]) => void):
      void {
    const pathParts = path.split('.');
    const traversedParts = [this.root.key];
    let node: ObserverNode|null = this.root;
    callback(node, false, traversedParts);

    while (pathParts.length > 0 && node !== null) {
      const currentPart = pathParts.shift()!;
      traversedParts.push(currentPart);
      node = node.children?.get(currentPart) || null;
      if (node !== null) {
        callback(node, pathParts.length === 0, traversedParts);
      }
    }
  }

  // Traverses all nodes from the given root node and below (including the root
  // itself) and invokes the callback on each node visited. Besides the
  // current node, it also passes the path from the original input `node` to the
  // current node.
  traverseTree(
      node: ObserverNode,
      callback: (node: ObserverNode, relativePath: string[]) => void): void {
    function visitNode(node: ObserverNode, relativePath: string[]) {
      callback(node, relativePath);

      if (node.children) {
        for (const child of node.children.values()) {
          visitNode(child, relativePath.concat(child.key));
        }
      }
    }

    visitNode(node, []);
  }

  addObserver(path: string, callback: ChangeCallback): number {
    let effectivePath = path;

    // Observers ending with '.*' receive notifications for any change
    // happening under the corresponding node.
    const isWildcard = path.endsWith('.*');
    if (isWildcard) {
      effectivePath = path.slice(0, -2);
    }

    const node = this.getNode(effectivePath, /*create=*/ true)!;
    if (!node.observers) {
      node.observers = new Set();
    }

    // Add observer to the ObserverNode.
    const id = ++this.nextObserverId_;
    node.observers.add({id, isWildcard, callback});

    // Add entry in `observers_` to be used in removeObserver.
    this.observers_.set(id, node);

    return id;
  }

  removeObserver(id: number): boolean {
    const node = this.observers_.get(id) || null;
    if (!node) {
      return false;
    }

    assert(node.observers);
    const observerEntry =
        Array.from(node.observers).find(node => node.id === id) || null;
    assert(observerEntry);

    this.observers_.delete(id);
    const deleted = node.observers.delete(observerEntry);
    assert(deleted);

    return true;
  }

  removeAllObservers() {
    for (const id of this.observers_.keys()) {
      this.removeObserver(id);
    }
  }
}

export class Observable<T extends Record<string, any>> {
  private proxyCache_: WeakMap<object, object> = new WeakMap();
  private proxy_: T;
  private target_: T;

  private observerTree_ = new ObserverTree();

  constructor(target: T) {
    this.target_ = target;
    this.proxy_ =
        buildProxy(target, this.onChange_.bind(this), [''], this.proxyCache_) as
        T;
  }

  getProxy(): T {
    return this.proxy_;
  }

  // Returns the original raw object. Useful when it needs to be serialized.
  getTarget(): T {
    return this.target_;
  }

  private onChange_(newValue: any, previousValue: any, path: string) {
    let lastNode: ObserverNode|null = null;

    this.observerTree_.traversePath(
        path, (node: ObserverNode, isLast: boolean, pathParts: string[]) => {
          if (isLast) {
            // Remember the last ObserverNode on 'path' for later.
            lastNode = node;
            return;
          }

          if (node.observers) {
            const base = getValueAtPath(pathParts.slice(1), this.proxy_);
            for (const {isWildcard, callback} of node.observers) {
              // Notify '.*' observers between the root and the modified node.
              // For wildcard observers above the changed node, report the
              // changed path and new values verbatim.
              if (isWildcard) {
                (callback as
                 WildcardChangeCallback)({path, value: newValue, base});
              }
            }
          }
        });

    if (lastNode === null) {
      // No observers exist. Nothing to do.
      return;
    }

    // Notify observers directly on the modified node or anywhere below it.
    this.observerTree_.traverseTree(
        lastNode, (node: ObserverNode, relativePath: string[]) => {
          if (!node.observers) {
            return;
          }

          let observerNewValue = newValue;
          let observerPreviousValue = previousValue;

          // Calculate the `newValue` and `previousValue` from each observer's
          // point of view.
          if (node !== lastNode) {
            observerNewValue = getValueAtPath(relativePath, newValue);
            observerPreviousValue = getValueAtPath(relativePath, previousValue);
          }

          const observedPath = [path, ...relativePath].join('.');

          for (const observer of node.observers) {
            if (observer.isWildcard) {
              if (node !== lastNode) {
                // For wildcard observers below the changed node, report the
                // observed path as 'path' and the relative new value as
                // 'value' and 'base'. This is to maintain parity with Polymer,
                // even though it is a bit odd.
                (observer.callback as WildcardChangeCallback)({
                  path: observedPath,
                  value: observerNewValue,
                  base: observerNewValue,
                });
              } else {
                // For wildcard observers at the changed node, report the
                // changed path as 'path' and the new value verbatim as
                // 'value'.
                (observer.callback as WildcardChangeCallback)(
                    {path, value: newValue, base: newValue});
              }
              continue;
            }

            // For non-wildcard observers below or at the changed node, report
            // the observed path as 'path' and the relative new value as
            // 'value'.
            observer.callback(
                observerNewValue, observerPreviousValue, observedPath);
          }
        });
  }

  addObserver(path: string, callback: ChangeCallback): number {
    return this.observerTree_.addObserver(path, callback);
  }

  removeObserver(id: number): boolean {
    return this.observerTree_.removeObserver(id);
  }

  removeAllObservers() {
    return this.observerTree_.removeAllObservers();
  }
}
