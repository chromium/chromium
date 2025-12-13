# Instructions for using the swift programming language in Chrome for iOS.

## Build rules

When adding or removing swift source code files, the build rules must be
updated. The BUILD.gn file in the same folder or in a parent folder of the
source file typically contains a build rule that includes a given source code
file.

### Swift-specific instructions

* Swift source files must be referenced by a `swift_source_set` build rule
  which defines a build target that contains only swift code.
* The build rule file must have
  `import("//build/config/apple/swift_source_set.gni")` to be able to use the
  `swift_source_set` rule.

## C++ interop

Use swift's direct C++ interop, if possible, whenever C++ code needs to be
invoked from swift. When needed, use the helpers in the C++ include file
`base/apple/swift_interop_util.h` to solve special cases.

### Instructions for calling C++ directly from swift

* Ensure that the C++ symbols are exported to swift using a modulemap.
* The swift modulemap should be defined in the same folder as the C++ build rule
  that builds the exported C++ symbol.
* In the module map, use nested modules when appropriate. The nesting hierarchy
  should reflect the nesting of the C++ namespaces that contain the exported
  symbols.
* In the `swift_source_set` rule that has a dependency on C++ code, use the
  `cxx_modulemap` argument to specify a module map that imports the required C++
  symbols.
* Add the necessary import statements to the swift source files to import the
  modules defined in the modulemap
* For an example of how to do this correctly, look at file
  `ios/chrome/test/swift_interop/BUILD.gn` and the files referenced by its build
  rules.
* Refer to the documentation for [C++ interop](https://www.swift.org/documentation/cxx-interop/#importing-c-into-swift)

## UI Programming instructions

* Prefer using SwiftUI for defining views. Only use UIKit when the required
  functionality is not exposed in SwiftUI.
* Use colors from the Chromium UI theme via the shims defined in file
  `ios/chrome/common/ui/colors/Color+Chrome.swift`.
* When image resources are needed in a view, import them using a variation of
  this syntax:

```swift
if let uiImage = UIImage(named: "<image_resource_name>") {
  Image(uiImage: uiImage)
}
```

* All UI strings must be imported using `NSLocalizedString`.
* The string resources for Chrome for iOS are defined in files
  `ios/chrome/app/strings/ios_strings.grd` and
  `ios/chrome/app/resources/chrome_localize_strings_config.plist`

## Delegate to the user

When you encounter non-trivial build errors related to swift/C++
interoperability, do not try to fix the problem yourself. Instead, annotate the
the code with comments that explain what you understand about the problem, send
a message to the user instructing them to fix the issue themselves, then quit.
