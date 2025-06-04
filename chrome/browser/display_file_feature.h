#ifndef CHROME_BROWSER_DISPLAY_FILE_FEATURE_H_
#define CHROME_BROWSER_DISPLAY_FILE_FEATURE_H_

#include "base/feature_list.h"
#include "base/files/file_path.h"

class DisplayFileFeature {
 public:
  // Checks if the feature is enabled
  static bool IsEnabled();

  // Reads content from the specified file
  static std::string ReadFromFile(const base::FilePath& file_path);

  // Writes content to the specified file
  static bool WriteToFile(const base::FilePath& file_path, const std::string& content);
};

#endif  // CHROME_BROWSER_DISPLAY_FILE_FEATURE_H_
