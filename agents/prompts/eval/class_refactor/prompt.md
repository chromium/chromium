Could you please refactor the `NestedStruct` from the `FakeSimpleClass`?
I'd like it to be its own class called `NestedClass`.

Please create new files `base/strings/nested_struct.h` and
`base/strings/nested_struct.cc` for the new `NestedClass`.

In the new `NestedClass`, please make the member variables private and create
public getter and setter methods for them. Also, please add a constructor to
initialize the members.

Next, please update `FakeSimpleClass` to use this new `NestedClass`.

Finally, please update the build files and compile the code to ensure that the
refactoring was successful.
