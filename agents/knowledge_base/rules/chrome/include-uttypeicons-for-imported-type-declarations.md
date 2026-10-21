---
name: chrome/include-uttypeicons-for-imported-type-declarations
rubric: correctness
severity: should_fix
---

**Rule.** When registering support for a standard or third-party document type
in macOS `CFBundleDocumentTypes`, declare its Uniform Type Identifier (UTI)
under `UTImportedTypeDeclarations` (matching Apple's CoreTypes specification).
In the `UTImportedTypeDeclarations` entry, always include a `UTTypeIcons`
dictionary containing `UTTypeIconBadgeName` set to `'Icon'` and `UTTypeIconText`
set to the uppercase format label (such as `'AVIF'`).

**Why.** macOS LaunchServices uses `CFBundleDocumentTypes` to route document
handling, but relies on `UTImportedTypeDeclarations` to resolve UTI tags and
display branded document icons. Standard file types already exist in macOS
CoreTypes, so declaring them in `UTImportedTypeDeclarations` is specifically
required to attach Chromium's branded document badge icon from the asset catalog
via `UTTypeIcons`; omitting either the declaration or `UTTypeIcons` leaves
document icons generic and unbadged.

## Examples

Bad:

```cpp
<!-- Bad: Registering CFBundleDocumentTypes without UTImportedTypeDeclarations -->
<key>CFBundleDocumentTypes</key>
<array>
	<dict>
		<key>CFBundleTypeRole</key>
		<string>Viewer</string>
		<key>LSItemContentTypes</key>
		<array>
			<string>public.avif</string>
		</array>
	</dict>
</array>

<!-- Bad: UTImportedTypeDeclarations omitting UTTypeIcons -->
<dict>
	<key>UTTypeConformsTo</key>
	<string>public.heif-standard</string>
	<key>UTTypeDescription</key>
	<string>AV1 Image File Format</string>
	<key>UTTypeIdentifier</key>
	<string>public.avif</string>
	<key>UTTypeTagSpecification</key>
	<dict>
		<key>com.apple.ostype</key>
		<string>AVIF</string>
		<key>public.filename-extension</key>
		<string>avif</string>
		<key>public.mime-type</key>
		<string>image/avif</string>
	</dict>
</dict>
```

Good:

```cpp
<!-- Good: Paired CFBundleDocumentTypes and UTImportedTypeDeclarations with UTTypeIcons -->
<key>CFBundleDocumentTypes</key>
<array>
	<dict>
		<key>CFBundleTypeRole</key>
		<string>Viewer</string>
		<key>LSItemContentTypes</key>
		<array>
			<string>public.avif</string>
		</array>
	</dict>
</array>
...
<key>UTImportedTypeDeclarations</key>
<array>
	<dict>
		<key>UTTypeConformsTo</key>
		<string>public.heif-standard</string>
		<key>UTTypeDescription</key>
		<string>AV1 Image File Format</string>
		<key>UTTypeIcons</key>
		<dict>
			<key>UTTypeIconBadgeName</key>
			<string>Icon</string>
			<key>UTTypeIconText</key>
			<string>AVIF</string>
		</dict>
		<key>UTTypeIdentifier</key>
		<string>public.avif</string>
		<key>UTTypeTagSpecification</key>
		<dict>
			<key>com.apple.ostype</key>
			<string>AVIF</string>
			<key>public.filename-extension</key>
			<string>avif</string>
			<key>public.mime-type</key>
			<string>image/avif</string>
		</dict>
	</dict>
</array>
```
