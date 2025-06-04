#!/usr/bin/env python
import time
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader, Subset
import torchvision.datasets as datasets
import torchvision.transforms as transforms

def train_model():
    # Use GPU if available; otherwise, use CPU.
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print("Using device:", device)
    
    # Define a transformation to convert images to tensors (and normalize to [0, 1]).
    transform = transforms.ToTensor()
    
    # Load the MNIST training dataset.
    train_dataset = datasets.MNIST(root='./data', train=True, download=True, transform=transform)
    
    # Mimic tfjs setup: use only a subset of 5500 samples.
    subset_indices = list(range(5500))
    train_subset = Subset(train_dataset, subset_indices)
    
    batch_size = 512
    train_loader = DataLoader(train_subset, batch_size=batch_size, shuffle=True)
    
    # Define a simple model: Flatten -> Dense(128, ReLU) -> Dense(10)
    model = nn.Sequential(
        nn.Flatten(),
        nn.Linear(28*28, 128),
        nn.ReLU(),
        nn.Linear(128, 10)
    )
    model.to(device)
    
    # Define loss and optimizer.
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters())
    
    epochs = 10
    print("Starting training...")
    start_time = time.perf_counter()
    for epoch in range(epochs):
        running_loss = 0.0
        correct = 0
        total = 0
        model.train()
        for images, labels in train_loader:
            images, labels = images.to(device), labels.to(device)
            optimizer.zero_grad()
            outputs = model(images)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()
            
            running_loss += loss.item() * images.size(0)
            _, predicted = torch.max(outputs, 1)
            total += labels.size(0)
            correct += (predicted == labels).sum().item()
        
        epoch_loss = running_loss / total
        epoch_acc = correct / total
        print(f"Epoch [{epoch+1}/{epochs}] Loss: {epoch_loss:.4f} Accuracy: {epoch_acc*100:.2f}%")
    end_time = time.perf_counter()
    training_time_ms = (end_time - start_time) * 1000
    print("Training complete.")
    print(f"Total training time: {training_time_ms:.2f} ms")
    print(f"Final training accuracy: {epoch_acc*100:.2f}%")

if __name__ == "__main__":
    train_model()
