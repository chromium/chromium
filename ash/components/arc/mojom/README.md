# Mojo interfaces for ARC

This directory contains the *.mojom files used for communication between Chrome
and ARC.
As the other end of IPCs is not in Chromium, there are a few caveats when
updating an interface in this directory.

## Versioning

Please follow the guidance in [the mojo doc](/mojo/public/tools/bindings/README.md#Versioning) in general.
Please also refer to [go/arc++ipc](http://go/arc++ipc).
You should keep it in mind that your change will not be applied atomically for
Chromium and ARC when modifying the IPC interfaces between Chrome and ARC.
Your change has to be backward compatible. In other words, Chrome with your
change should run fine with ARC without it, and vice versa.

### Adding a new method to an existing interface

All methods in an interface should have the fixed ordinal values which refers
to the relative positional layout in the interface to make it
backward-compatible easily.
An interface should have the comment explaining the next available ordinal
value for a new method.
Also there should be a comment on top of the file which explans what version
number should be used for your new method.

For example, if you are going to add a new method `YourNewMethod` to the
following file:
```
// Next MinVersion: 5

...

// Next method ID: 10
interface SomeArcHost {
  ...
}
```

You should update the file like the following:

```
// Next MinVersion: 6

...

// Next method ID: 11
interface SomeArcHost {
  [MinVersion=5] YourNewMethod@10();
  ...
}
```

Please don't forget to increment the numbers after adding your new method.

### Deprecating a method

You cannot remove a method immediately when it becomes obsolete because an old
version of ARC may call it.
As a first step, you can change the method name to show it's deprecated.
Unless you change the ordinal value of the method, ARC is still able to call it
because the relative layout in the interface is independent from the name.
Please refer to [the mojo doc](/mojo/public/tools/bindings/README.md) for details.

For example, when you want to deprecate the method, `YourNewMethod`, you can
rename it to `DEPRECATED_YourNewMethod`.

```
// Next method ID: 11
interface SomeArcHost {
  [MinVersion=5] DEPRECATED_YourNewMethod@10();
  ...
}
```

Then, you can start removing all callers of the method.
Please be careful not to break the compatibility.

### Removing a deprecated method

It's safe to remove a deprecated method once all callers of it are removed on
the stable channel.
In this case, you can clean up the deprecated method by removing it and leaving
a comment not to reuse the ordinal value of it.
If any method in the interface doesn't have an ordinal value, you should add it
to all methods, so that removing the deprecated method doesn't implicitly
re-order the later ones.

```
// Deprecated method IDs: 10
// Next method ID: 11
interface SomeArcHost {
  // REMOVED! [MinVersion=5] DEPRECATED_YourNewMethod@10();
  ...
}
```

### Version guard in code

For Chromium C++ code, `ARC_GET_INSTANCE_FOR_METHOD` macro is provided in
[connection_holder.h](https://source.chromium.org/chromium/chromium/src/+/main:ash/components/arc/session/connection_holder.h;l=24;drc=eeb36b2554f18c2239fd8fc1daeb8c020c358a55).
It returns `nullptr` when the remote side doesn't support the given method.
The common pattern to call a remote method in Chromium C++ is like the
following:

```c++
auto* instance = ARC_GET_INSTANCE_FOR_METHOD(service, MethodName);
if (!instance)
  return;
instnace->MethodName(param1, param2);
```

For Java code inside ARC, there is no useful macro unfortunately.
You have to keep track of minimum version of the instance for each method and
check the version before calling a remote method.

For both, it's considered safe to remove the version guard once a method is
available on all release channels.

