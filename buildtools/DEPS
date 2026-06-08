use_relative_paths = True

# Only these hosts are allowed for dependencies in this DEPS file.
# If you need to add a new host, contact chrome infrastracture team.
allowed_hosts = [
  'chromium-clang-format',
]

deps = {
  'win-format': {
    'bucket': 'chromium-clang-format',
    'condition': 'host_os == "win"',
    'dep_type': 'gcs',
    'objects': [
      {
        'object_name': 'fb0bcaf406ad6f0bd6b4a6347d2abe78a94fb13e',
        'sha256sum': 'ee7a1f89733de23daf8ee035f8aebafa52410c9f4dd1f05ee12a95ce517a0953',
        'size_bytes': 3799552,
        'generation': 1779297371701340,
        'output_file': 'clang-format.exe',
      },
    ],
  },
  'mac-format': {
    'bucket': 'chromium-clang-format',
    'condition': 'host_os == "mac" and host_cpu == "x64"',
    'dep_type': 'gcs',
    'objects': [
      {
        'object_name': '35ddc571ab654a4f58d7b35f394b04835d0b98f9',
        'sha256sum': '39a9c4e138a8c5e6ab5cd7b21321c2ca03f3f518a6ed07de82162d9f4f37318f',
        'size_bytes': 3567528,
        'generation': 1779297382681217,
        'output_file': 'clang-format',
      },
    ],
  },
  'mac_arm64-format': {
    'bucket': 'chromium-clang-format',
    'condition': 'host_os == "mac" and host_cpu == "arm64"',
    'dep_type': 'gcs',
    'objects': [
      {
        'object_name': '945e45cbcb5443c3e569d5e0783fd012154fc4a7',
        'sha256sum': 'a1fb17260eda2971f52ed341501b93703b0fce69ffb43a179b891b66f6f63e5a',
        'size_bytes': 3245456,
        'generation': 1779297393605542,
        'output_file': 'clang-format',
      },
    ],
  },
  'linux64-format': {
    'bucket': 'chromium-clang-format',
    'condition': 'host_os == "linux"',
    'dep_type': 'gcs',
    'objects': [
      {
        'object_name': 'f5485c39451137ee943021a0fb63738b306c5026',
        'sha256sum': '5d384f2a3e72f01c5c61dd07e3a2577723cfa20c8c019131467f00fb6da8a75c',
        'size_bytes': 3764032,
        'generation': 1779297360709713,
        'output_file': 'clang-format',
      },
    ],
  },
}
