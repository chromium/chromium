// no need for this

#include "chrome/browser/display_file_feature.h"
#include "base/feature_list.h"
#include "content/public/common/content_features.h"
#include "base/files/file_util.h"
#include "base/logging.h" // Make sure to include the logging header

bool DisplayFileFeature::IsEnabled() {
  bool is_enabled = base::FeatureList::IsEnabled(features::kDisplayFileContentFeature);
  LOG(INFO) << "DisplayFileFeature::IsEnabled - Feature enabled status: " << is_enabled;
  return is_enabled;
}

std::string DisplayFileFeature::ReadFromFile(const base::FilePath& file_path) {
  std::string content;
  if (IsEnabled() && base::ReadFileToString(file_path, &content)) {
    LOG(INFO) << "DisplayFileFeature::ReadFromFile - Successfully read from file: " << file_path;
    return content;
  }
  LOG(WARNING) << "DisplayFileFeature::ReadFromFile - Failed to read from file or feature disabled: " << file_path;
  return "";  // Return empty string if feature is disabled or read fails
}

bool DisplayFileFeature::WriteToFile(const base::FilePath& file_path, const std::string& content) {
  if (IsEnabled() && base::WriteFile(file_path, content.c_str(), content.size())) {
    LOG(INFO) << "DisplayFileFeature::WriteToFile - Successfully wrote to file: " << file_path;
    return true;
  }
  LOG(WARNING) << "DisplayFileFeature::WriteToFile - Failed to write to file or feature disabled: " << file_path;
  return false;
}
