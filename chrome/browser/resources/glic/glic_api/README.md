# Glic web client API

This directory contains the API that is consumed by the web client loaded in the
glic webview. It cannot be freely moved or renamed. Modifications must be made
in a coordinated way to avoid breakage.

## Removing unused API properties

Follow the [internal instructions](http://shortn/_Ok6zk7DElM) to confirm that
API removal is appropriate and safe. Once confirmed, proceed to find all
references to the API property. Removed names and identifiers should be marked
as reserved to avoid them being reused in the future. For instance, in
[`glic_api.ts`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/glic/glic_api/glic_api.ts)
replace the current declaration with:

```javascript
interface GlicBrowserHost {
  ... // Main interface definition section

  // Removed fields and methods:
  someRemovedFunction?(): never;
  ... // More removed fields and methods
}
```

In the request types list from
[`request_types.ts`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/glic/glic_api_impl/request_types.ts),
reserve the previously used identifier:

```javascript
export const HOST_REQUEST_TYPES: HostRequestEnumNamesType&{MAX_VALUE: number} =
    (() => {
      const result = {
        ...
        OnTurnCompleted: 43,
        // Do not reuse deleted request ID: 44,
        ScrollTo: 45,
        ...
      };
      return {...result, MAX_VALUE: Math.max(...Object.values(result))};
    })();
```